#include <assert.h>
#include <fstream>
#include <iostream>
#include <signal.h>

#include "std/foreach.h"

#include "cli/shell_font.h"

#include "atframe/atapp.h"

namespace atapp {
    app *app::last_instance_;

    app::app() : mode_(mode_t::CUSTOM) {
        last_instance_ = this;
        conf_.id = 0;
        conf_.execute_path = NULL;
        conf_.stop_timeout = 30000; // 30s
        conf_.tick_interval = 32;   // 32ms

        tick_timer_.sec_update = util::time::time_utility::raw_time_t::zero();
        tick_timer_.sec = 0;
        tick_timer_.usec = 0;
    }

    app::~app() {
        if (this == last_instance_) {
            last_instance_ = NULL;
        }
    }

    int app::run(atbus::adapter::loop_t *ev_loop, int argc, const char *argv[], void *priv_data) {
        if (check(flag_t::RUNNING)) {
            return 0;
        }

        // step 1. bind default options
        // step 2. load options from cmd line
        setup_option(argc, argv, priv_data);
        setup_command();

        // step 3. if not in show mode, exit 0
        if (mode_t::INFO == mode_) {
            return 0;
        }

        // step 4. load options from cmd line
        int ret = reload();
        if (ret < 0) {
            return ret;
        }

        // step 5. if not in start mode, send cmd
        switch (mode_) {
        case mode_t::START: {
            break;
        }
        case mode_t::CUSTOM:
        case mode_t::STOP:
        case mode_t::RELOAD: {
            return send_last_command(ev_loop);
        }
        default: { return 0; }
        }

        // step 6. setup log & signal
        setup_log();
        setup_signal();
        setup_atbus();
        setup_timer();

        // step 7. all modules init
        owent_foreach(module_ptr_t & mod, modules_) { mod->init(); }

        // step 8. all modules reload
        owent_foreach(module_ptr_t & mod, modules_) { mod->reload(); }

        // step 9. set running
        return run_ev_loop(ev_loop);
    }

    int app::reload() {
        app_conf old_conf = conf_;
        WLOGINFO("============ start to load configure ============");
        // step 1. reset configure
        cfg_loader_.clear();

        // step 2. reload from program configure file
        if (conf_.conf_file.empty()) {
            ss() << util::cli::shell_font_style::SHELL_FONT_COLOR_RED << "missing configure file" << std::endl;
            print_help();
            return -1;
        }
        if (cfg_loader_.load_file(conf_.conf_file.c_str(), false) < 0) {
            ss() << util::cli::shell_font_style::SHELL_FONT_COLOR_RED << "load configure file " << conf_.conf_file << " failed"
                 << std::endl;
            print_help();
            return -1;
        }

        // step 3. reload from external configure files
        // step 4. merge configure
        {
            std::vector<std::string> external_confs;
            cfg_loader_.dump("atapp.config.external", external_confs);
            owent_foreach(std::string & conf_fp : external_confs) {
                if (!conf_fp.empty()) {
                    if (cfg_loader_.load_file(conf_.conf_file.c_str(), true) < 0) {
                        if (check(flag_t::RUNNING)) {
                            WLOGERROR("load external configure file %s failed", conf_fp.c_str());
                        } else {
                            ss() << util::cli::shell_font_style::SHELL_FONT_COLOR_RED << "load external configure file " << conf_fp
                                 << " failed" << std::endl;
                            return 1;
                        }
                    }
                }
            }
        }

        // step 5. if not in start mode, return
        if (mode_t::START != mode_) {
            return 0;
        }

        if (check(flag_t::RUNNING)) {
            // step 6. reset log
            setup_log();

            // step 7. if inited, let all modules reload
            owent_foreach(module_ptr_t & mod, modules_) { mod->reload(); }

            // step 8. if running and tick interval changed, reset timer
            if (old_conf->tick_interval != conf_->tick_interval) {
                setup_timer();
            }
        }

        WLOGINFO("------------ load configure done ------------");
        return 0;
    }

    int app::stop() {
        WLOGINFO("============ receive stop signal and ready to stop all services ============");
        // step 1. set stop flag.
        bool is_stoping = set_flag(flag_t::STOPING, true);

        // step 2. stop libuv and return from uv_run
        if (!is_stoping) {
            uv_stop(bus_node_.get_evloop());
        }
        return 0;
    }

    int app::tick() {
        int active_count;
        util::time::time_utility::update();
        // record start time point
        util::time::time_utility::raw_time_t start_tp = util::time::time_utility::now();

        do {
            if (tick_timer_.sec != util::time::time_utility::get_now()) {
                tick_timer_.sec = util::time::time_utility::get_now();
                tick_timer_.usec = 0;
                tick_timer_.sec_update = util::time::time_utility::now();
            } else {
                tick_timer_.usec = static_cast<time_t>(
                    std::chrono::duration_cast<std::chrono::microseconds>(util::time::time_utility::now() - tick_timer_.sec_update)
                        .count());
            }

            active_count = 0;
            int res;
            // step 1. proc available modules
            owent_foreach(module_ptr_t & mod, modules_) {
                res = mod->tick();
                if (res < 0) {
                    WLOGERROR("module %s run tick and return %d", mod->name(), res);
                } else {
                    active_count += res;
                }
            }

            // step 2. proc atbus
            res = bus_node_->proc(tick_timer_.sec, tick_timer_.usec);
            if (res < 0) {
                WLOGERROR("atbus run tick and return %d", res);
            } else {
                active_count += res;
            }

            // only tick time less than tick interval will run loop again
            util::time::time_utility::update();
            util::time::time_utility::raw_time_t end_tp = util::time::time_utility::now();
        } while (active_count > 0 && (end_tp - start_tp) >= std::chrono::milliseconds(conf_.tick_interval));
        return 0;
    }

    bool app::check(flag_t::type f) const {
        if (f < 0 || f >= flag_t::FLAG_MAX) {
            return false;
        }

        return flags_.test(f);
    }

    void app::add_module(module_ptr_t module) {
        if (this == module->owner_) {
            return;
        }

        assert(NULL == module->owner_);

        modules_.push_back(module);
        module->owner_ = this;
    }

    util::cli::cmd_option_ci::ptr_type app::get_command_manager() {
        if (!cmd_handler_) {
            return cmd_handler_ = util::cli::cmd_option_ci::create();
        }

        return cmd_handler_;
    }

    util::cli::cmd_option::ptr_type app::get_option_manager() {
        if (!app_option_) {
            return app_option_ = util::cli::cmd_option::create();
        }

        return app_option_;
    }

    void app::set_app_version(const std::string &ver) { conf_.app_version = ver; }

    const std::string &app::get_app_version() const { return conf_.app_version； }

    bool app::set_flag(flag_t::type f, bool v) {
        if (f < 0 || f >= flag_t::FLAG_MAX) {
            return false;
        }

        bool ret = flags_.test(f);
        flags_.set(f, v);
        return ret;
    }

    int app::run_ev_loop(atbus::adapter::loop_t *ev_loop) {
        set_flag(flag_t::RUNNING, true);
        // write pid file
        if (!conf_.pid_file.empty()) {
            std::fstream pid_file;
            pid_file.open(conf_.pid_file.c_str(), std::ios::out);
            if (!pid_file.is_open()) {
                ss() << util::cli::shell_font_style::SHELL_FONT_COLOR_RED << "open and write pif file " << conf_.pid_file << " failed"
                     << std::endl;
                return -1;
            }

            pid_file << atbus::node::get_pid();
            pid_file.close();
        }


        // TODO step X. loop uv_run util stop flag is set
        // TODO step X. notify all modules to finish and wait for all modules stop
        // TODO step X. if stop is blocked, setup stop timeout and waiting for all modules finished
        return 0;
    }

    // graceful Exits
    static void _app_setup_signal_term(int signo) {
        if (NULL != app::last_instance_) {
            app::last_instance_->stop();
        }
    }

    void app::setup_signal() {
        // block signals
        detail::last_atapp_ = this;
        signal(SIGTERM, _app_setup_signal_term);

#ifndef _MSC_VER
        signal(SIGHUP, SIG_IGN);  // lost parent process
        signal(SIGPIPE, SIG_IGN); // close stdin, stdout or stderr
        signal(SIGTSTP, SIG_IGN); // close tty
        signal(SIGTTIN, SIG_IGN); // tty input
        signal(SIGTTOU, SIG_IGN); // tty output
#endif
    }

    void app::setup_log() {}

    void app::setup_atbus() {}

    void app::setup_timer() {}

    void app::print_help() const {
        printf("Usage: %s <options> <command> [command paraters...]\n", conf_.execute_path);
        printf("%s\n", get_option_manager()->get_help_msg().c_str());
    }

    int app::prog_option_handler_help(util::cli::callback_param params, util::cli::cmd_option *opt_mgr) {
        assert(opt_mgr);
        mode_ = mode_t::INFO;
        printf("Usage: %s <options> <command> [command paraters...]\n", conf_.execute_path);
        printf("%s\n", opt_mgr->get_help_msg().c_str());
        return 0;
    }

    int app::prog_option_handler_version(util::cli::callback_param params) {
        mode_ = mode_t::INFO;
        if (get_app_version().empty()) {
            printf("1.0.0.0 - based on libatapp %s\n", LIBATAPP_VERSION);
        } else {
            printf("%s - based on libatapp %s\n", get_app_version().c_str(), LIBATAPP_VERSION);
        }
        return 0;
    }

    int app::prog_option_handler_set_id(util::cli::callback_param params) {
        if (params.get_params_number() > 0) {
            conf_.id = params[0]->as<app_id_t>();
        } else {
            util::cli::shell_stream ss(std::cerr);
            ss() << util::cli::shell_font_style::SHELL_FONT_COLOR_RED << "-id require 1 parameter" << std::endl;
        }

        return 0;
    }

    int app::prog_option_handler_set_conf_file(util::cli::callback_param params) {
        if (params.get_params_number() > 0) {
            conf_.conf_file = params[0]->as_cpp_string();
        } else {
            util::cli::shell_stream ss(std::cerr);
            ss() << util::cli::shell_font_style::SHELL_FONT_COLOR_RED << "-c, --conf, --config require 1 parameter" << std::endl;
        }

        return 0;
    }

    int app::prog_option_handler_set_pid(util::cli::callback_param params) {
        if (params.get_params_number() > 0) {
            conf_.pid_file = params[0]->as_cpp_string();
        } else {
            util::cli::shell_stream ss(std::cerr);
            ss() << util::cli::shell_font_style::SHELL_FONT_COLOR_RED << "-p, --pid require 1 parameter" << std::endl;
        }

        return 0;
    }

    int app::prog_option_handler_resume_mode(util::cli::callback_param params) {
        conf_.resume_mode = true;
        return 0;
    }

    int app::prog_option_handler_start(util::cli::callback_param params) {
        mode_ = mode_t::START;
        return 0;
    }

    int app::prog_option_handler_stop(util::cli::callback_param params) {
        mode_ = mode_t::STOP;
        last_command_.clear();
        last_command_.push_pack("stop");
        return 0;
    }

    int app::prog_option_handler_reload(util::cli::callback_param params) {
        mode_ = mode_t::RELOAD;
        last_command_.clear();
        last_command_.push_pack("reload");
        return 0;
    }

    int app::prog_option_handler_run(util::cli::callback_param params) {
        mode_ = mode_t::CUSTOM;
        for (size_t i = 0; i < params.get_params_number(); ++i) {
            last_command_.push_pack(params[i]->as_cpp_string());
        }

        if (0 == params.get_params_number()) {
            mode_ = mode_t::INFO;
            util::cli::shell_stream ss(std::cerr);
            ss() << util::cli::shell_font_style::SHELL_FONT_COLOR_RED << "run must follow a command" << std::endl;
        }
        return 0;
    }

    void app::setup_option(int argc, const char *argv[], void *priv_data) {
        assert(argc > 0);

        util::cli::cmd_option::ptr_type opt_mgr = get_option_manager();
        // show help and exit
        opt_mgr->bind_cmd("-h, --help, help", &app::prog_option_handler_help, this, &opt_mgr)
            ->set_help_msg("-h. --help, help                       show this help message.");

        // show version and exit
        opt_mgr->bind_cmd("-v, --version", &app::prog_option_handler_version, this)
            ->set_help_msg("-v, --version                          show version and exit.");

        // set app bus id
        opt_mgr->bind_cmd("-id", &app::prog_option_handler_set_id, this)
            ->set_help_msg("-id <bus id>                           set app bus id.");

        // set configure file path
        opt_mgr->bind_cmd("-c, --conf, --config", &app::prog_option_handler_set_conf_file, this)
            ->set_help_msg("-c, --conf, --config <file path>       set configure file path.");

        // set app pid file
        opt_mgr->bind_cmd("-p, --pid", &app::prog_option_handler_set_pid, this)
            ->set_help_msg("-p, --pid <pid file>                   set where to store pid.");

        // set configure file path
        opt_mgr->bind_cmd("-r, --resume", &app::prog_option_handler_resume_mode, this)
            ->set_help_msg("-r, --resume                           try to resume when start.");

        // start server
        opt_mgr->bind_cmd("start", &app::prog_option_handler_start, this)
            ->set_help_msg("start                                  start mode.");

        // stop server
        opt_mgr->bind_cmd("stop", &app::prog_option_handler_stop, this)
            ->set_help_msg("stop                                   send stop command to server.");

        // reload all configures
        opt_mgr->bind_cmd("reload", &app::prog_option_handler_reload, this)
            ->set_help_msg("reload                                 send reload command to server.");

        // run custom command
        opt_mgr->bind_cmd("run", &app::prog_option_handler_run, this)
            ->set_help_msg("run <command> [parameters...]          send custom command and parameters to server.");

        conf_.execute_path = argv[0];
        opt_mgr->start(argc - 1, argv, false, priv_data);
    }

    int app::app::command_handler_start(util::cli::callback_param params) {
        // do nothing
        return 0;
    }

    int app::command_handler_stop(util::cli::callback_param params) { stop(); }

    int app::command_handler_reload(util::cli::callback_param params) { reload(); }

    int app::command_handler_invalid(util::cli::callback_param params) {
        WLOGERROR("receive invalid command %s", par.get("@Cmd")->to_string());
    }

    void app::setup_command() {
        util::cli::cmd_option::ptr_type opt_mgr = get_option_manager();
        // dump all connected nodes to default log collector
        // opt_mgr->bind_cmd("dump");
        // dump all nodes to default log collector
        // opt_mgr->bind_cmd("dump");
        // dump state

        // start server
        opt_mgr->bind_cmd("start", &app::app::command_handler_start, this);
        // stop server
        opt_mgr->bind_cmd("stop", &app::app::command_handler_stop, this);
        // reload all configures
        opt_mgr->bind_cmd("reload", &app::app::command_handler_reload, this);

        // invalid command
        opt_mgr->bind_cmd("@OnError", &app::command_handler_invalid, this);
    }

    int app::send_last_command(atbus::adapter::loop_t *ev_loop) {
        // TODO step 1. using the fastest way to connect to server
        // TODO step 2. connect failed return error code
        // TODO step 3. waiting for connect success
        // TODO step 4. send data
        // TODO step 5. setup timeout timer
        // TODO step 6. waiting for send done(for shm, no need to wait, for io_stream fd, waiting write callback)
        return 0;
    }
}
