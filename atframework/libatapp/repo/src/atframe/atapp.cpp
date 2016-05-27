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

        tick_timer_.tick_timer.is_activited = false;
        tick_timer_.timeout_timer.is_activited = false;
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
        conf_.bus_node_.ev_loop = ev_loop;
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

        // step 7. all modules init
        owent_foreach(module_ptr_t & mod, modules_) {
            if (mod->is_enabled()) {
                mod->init();
            }
        }

        // step 8. all modules reload
        owent_foreach(module_ptr_t & mod, modules_) {
            if (mod->is_enabled()) {
                mod->reload();
            }
        }

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

        // apply ini configure
        apply_configure();

        // step 5. if not in start mode, return
        if (mode_t::START != mode_) {
            return 0;
        }

        if (check(flag_t::RUNNING)) {
            // step 6. reset log
            setup_log();

            // step 7. if inited, let all modules reload
            owent_foreach(module_ptr_t & mod, modules_) {
                if (mod->is_enabled()) {
                    mod->reload();
                }
            }

            // step 8. if running and tick interval changed, reset timer
            if (old_conf->tick_interval != conf_->tick_interval) {
                setup_timer();
            }
        }

        WLOGINFO("------------ load configure done ------------");
        return 0;
    }

    int app::stop() {
        WLOGINFO("============ receive stop signal and ready to stop all modules ============");
        // step 1. set stop flag.
        set_flag(flag_t::STOPING, true);
        // bool is_stoping = set_flag(flag_t::STOPING, true);

        // step 2. stop libuv and return from uv_run
        // if (!is_stoping) {
        uv_stop(bus_node_.get_evloop());
        //}
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
                if (mod->is_enabled()) {
                    res = mod->tick();
                    if (res < 0) {
                        WLOGERROR("module %s run tick and return %d", mod->name(), res);
                    } else {
                        active_count += res;
                    }
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

    void app::ev_stop_timeout(uv_timer_t *handle) {
        assert(handle);
        assert(handle->data);

        app *self = reinterpret_cast<app *>(handle->data);
        self->set_flag(flag_t::TIMEOUT, true);
        uv_stop(handle->loop);
    }

    bool app::set_flag(flag_t::type f, bool v) {
        if (f < 0 || f >= flag_t::FLAG_MAX) {
            return false;
        }

        bool ret = flags_.test(f);
        flags_.set(f, v);
        return ret;
    }

    int app::apply_configure() {
        // id
        if (0 == conf_.id) {
            cfg_loader_.dump_to("atapp.id", conf_.id);
        }

        // hostname
        {
            std::string hostname;
            cfg_loader_.dump_to("atapp.hostname", hostname);
            if (!hostname.empty()) {
                atbus::node::set_hostname(hostname);
            }
        }

        conf_.bus_listen.clear();
        cfg_loader_.dump_to("atapp.bus.listen", conf_.bus_listen);

        // conf_.stop_timeout = 30000; // use last available value
        cfg_loader_.dump_to("atapp.timer.stop_timeout", conf_.stop_timeout);

        // conf_.tick_interval = 32; // use last available value
        cfg_loader_.dump_to("atapp.timer.tick_interval", conf_.tick_interval);

        // atbus configure
        atbus::node::default_conf(&conf_.bus_node_);

        cfg_loader_.dump_to("atapp.bus.children_mask", conf_.bus_node_.children_mask);
        {
            bool optv = false;
            cfg_loader_.dump_to("atapp.bus.options.global_router", optv);
            conf_.bus_node_.flags.set(atbus::node::conf_flag_t::EN_CONF_GLOBAL_ROUTER, optv);
        }

        cfg_loader_.dump_to("atapp.bus.proxy", conf_.bus_node_.father_address);
        cfg_loader_.dump_to("atapp.bus.loop_times", conf_.bus_node_.loop_times);
        cfg_loader_.dump_to("atapp.bus.ttl", conf_.bus_node_.ttl);
        cfg_loader_.dump_to("atapp.bus.backlog", conf_.bus_node_.backlog);
        cfg_loader_.dump_to("atapp.bus.first_idle_timeout", conf_.bus_node_.first_idle_timeout);
        cfg_loader_.dump_to("atapp.bus.ping_interval", conf_.bus_node_.ping_interval);
        cfg_loader_.dump_to("atapp.bus.retry_interval", conf_.bus_node_.retry_interval);
        cfg_loader_.dump_to("atapp.bus.fault_tolerant", conf_.bus_node_.fault_tolerant);
        cfg_loader_.dump_to("atapp.bus.msg_size", conf_.bus_node_.msg_size);
        cfg_loader_.dump_to("atapp.bus.recv_buffer_size", conf_.bus_node_.recv_buffer_size);
        cfg_loader_.dump_to("atapp.bus.send_buffer_size", conf_.bus_node_.send_buffer_size);
        cfg_loader_.dump_to("atapp.bus.send_buffer_number", conf_.bus_node_.send_buffer_number);

        return 0;
    }

    int app::run_ev_loop(atbus::adapter::loop_t *ev_loop) {
        bool keep_running = true;

        set_flag(flag_t::RUNNING, true);

        if (setup_timer() < 0) {
            set_flag(flag_t::RUNNING, false);
            return -1;
        }

        // write pid file
        if (!conf_.pid_file.empty()) {
            std::fstream pid_file;
            pid_file.open(conf_.pid_file.c_str(), std::ios::out);
            if (!pid_file.is_open()) {
                ss() << util::cli::shell_font_style::SHELL_FONT_COLOR_RED << "open and write pif file " << conf_.pid_file << " failed"
                     << std::endl;

                // failed and skip running
                keep_running = false;
            } else {
                pid_file << atbus::node::get_pid();
                pid_file.close();
            }
        }

        while (keep_running) {
            // step X. loop uv_run util stop flag is set
            uv_run(bus_node_.get_evloop(), UV_RUN_DEFAULT);
            if (check_flag(flag_t::STOPING)) {
                keep_running = false;

                if (check_flag(flag_t::TIMEOUT)) {
                    // step X. notify all modules timeout
                    owent_foreach(module_ptr_t & mod, modules_) {
                        if (mod->is_enabled()) {
                            WLOGERROR("try to stop module %s but timeout", mod->name());
                            mod->timeout();
                            mod->disable();
                        }
                    }
                } else {
                    // step X. notify all modules to finish and wait for all modules stop
                    owent_foreach(module_ptr_t & mod, modules_) {
                        if (mod->is_enabled()) {
                            int res = mod->stop();
                            if (0 == res) {
                                mod->disable();
                            } else if (res < 0) {
                                mod->disable();
                                WLOGERROR("try to stop module %s but failed and return %d", mod->name(), res);
                            } else {
                                // any module stop running will make app wait
                                keep_running = true;
                            }
                        }
                    }

                    // step X. if stop is blocked and timeout not triggered, setup stop timeout and waiting for all modules finished
                    if (false == tick_timer_.timeout_timer.is_activited) {
                        uv_timer_init(bus_node_.get_evloop(), &tick_timer_.timeout_timer.timer);
                        tick_timer_.timeout_timer.timer.data = this;

                        int res = uv_timer_start(&tick_timer_.timeout_timer.timer, ev_stop_timeout, conf_.stop_timeout, 0);
                        if (0 == res) {
                            tick_timer_.timeout_timer.is_activited = true;
                        } else {
                            WLOGERROR("setup stop timeout failed, res: %d", res);
                            set_flag(flag_t::TIMEOUT, false);
                        }
                    }
                }
            }
        }

        // close timer
        close_timer(tick_timer_.tick_timer);
        close_timer(tick_timer_.timeout_timer);

        // not running now
        set_flag(flag_t::RUNNING, false);
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

    void app::setup_log() {
        // register inner log module
        if (log_reg_.find(log_sink_maker::get_file_sink_name()) == log_reg_.end()) {
            log_reg_[log_sink_maker::get_file_sink_name()] = log_sink_maker::get_file_sink_reg();
        }

        if (log_reg_.find(log_sink_maker::get_stdout_sink_name()) == log_reg_.end()) {
            log_reg_[log_sink_maker::get_stdout_sink_name()] = log_sink_maker::get_stdout_sink_reg();
        }

        if (log_reg_.find(log_sink_maker::get_stderr_sink_name()) == log_reg_.end()) {
            log_reg_[log_sink_maker::get_stderr_sink_name()] = log_sink_maker::get_stderr_sink_reg();
        }

        // load configure
        uint32_t log_cat_number = LOG_WRAPPER_CATEGORIZE_SIZE;
        cfg_loader_.dump_to("atapp.log.cat.number", log_cat_number);
        if (log_cat_number > LOG_WRAPPER_CATEGORIZE_SIZE) {
            ss() << util::cli::shell_font_style::SHELL_FONT_COLOR_RED << "log categorize should not be greater than "
                 << LOG_WRAPPER_CATEGORIZE_SIZE << ". you can define LOG_WRAPPER_CATEGORIZE_SIZE to a greater number and rebuild atapp."
                 << std::endl;
            log_cat_number = LOG_WRAPPER_CATEGORIZE_SIZE;
        }
        int log_level_id = util::log::log_wrapper::level_t::LOG_LW_INFO;
        cfg_loader_.dump_to("atapp.log.level", log_level_id);

        char log_path[256] = {0};

        for (uint32_t i = 0; i < log_cat_number; ++i) {
            std::string log_name, log_prefix;
            UTIL_STRFUNC_SNPRINTF(log_path, sizeof(log_path), "atapp.log.cat.%u.name", i);
            cfg_loader_.dump_to(log_path, log_name);

            if (log_name.empty()) {
                continue;
            }

            UTIL_STRFUNC_SNPRINTF(log_path, sizeof(log_path), "atapp.log.cat.%u.prefix", i);
            cfg_loader_.dump_to(log_path, log_prefix);

            // init and set prefix
            WLOG_INIT(i, WLOG_LEVELID(log_level_id));
            if (!log_prefix.empty()) {
                WLOG_GETCAT(i)->set_prefix_format(log_prefix);
            }

            // TODO For now, log can not be reload. we may make it available someday in the future
            if (!WLOG_GETCAT(i)->get_sinks().empty()) {
                continue;
            }

            // register log handles
            for (uint32_t j = 0;; ++j) {
                std::string sink_type;
                UTIL_STRFUNC_SNPRINTF(log_path, sizeof(log_path), "atapp.log.%s.%u.type", log_name.c_str(), j);
                cfg_loader_.dump_to(log_path, sink_type);

                if (sink_type.empty()) {
                    break;
                }

                // already read log cat name, sink type name
                UTIL_STRFUNC_SNPRINTF(log_path, sizeof(log_path), "atapp.log.%s.%u", log_name.c_str(), j);
                util::config::ini_value &cfg_set = cfg_loader_.get_node(log_path);

                // register log sink
                std::map<std::string, log_sink_maker::log_reg_t>::iterator iter = log_reg_.find(sink_type);
                if (iter != log_reg_.end()) {
                    iter->second(log_name, *WLOG_GETCAT(i), j, cfg_set);
                } else {
                    ss() << util::cli::shell_font_style::SHELL_FONT_COLOR_RED << "unavailable log type " << sink_type
                         << ", you can add log type register handle before init." << std::endl;
                }
            }
        }
    }

    void app::setup_atbus() {
        // TODO if in resume mode, try resume shm channel first
        // TODO init listen

        // TODO if has father node, block and connect to father node

        // TODO setup recv callback
        // TODO setup error callback
        // TODO setup send failed callback
        // TODO setup custom cmd callback
    }

    void app::close_timer(timer_info_t &t) {
        if (t.is_activited) {
            uv_timer_stop(t.timer);
            uv_close(&t.timer, NULL);
            t.is_activited = false;
        }
    }

    static void _app_tick_timer_handle(uv_timer_t *handle) {
        assert(handle);
        assert(handle->data);

        app *self = reinterpret_cast<app *>(handle->data);
        self->tick();
    }

    int app::setup_timer() {
        close_timer(tick_timer_.tick_timer);

        if (conf_.tick_interval < 4) {
            conf_.tick_interval = 4;
            WLOGWARNING("tick interval can not smaller than 4ms, we use 4ms now.");
        }

        uv_timer_init(bus_node_.get_evloop(), &tick_timer_.tick_timer.timer);
        tick_timer_.tick_timer.timer.data = this;

        int res = uv_timer_start(&tick_timer_.tick_timer.timer, _app_tick_timer_handle, conf_.tick_interval, conf_.tick_interval);
        if (0 == res) {
            tick_timer_.tick_timer.is_activited = true;
        } else {
            WLOGERROR("setup tick timer failed, res: %d", res);
            return -1;
        }

        return 0;
    }

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
