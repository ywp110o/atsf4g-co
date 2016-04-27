#include <iostream>
#include <signal.h>

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
        set_flag(flag_t::RUNNING, true);

        // step 1. bind default options
        setup_option(argc, argv, priv_data);
        setup_command();

        // step 2. load options from cmd line
        cmd_opts_.start(argc, argv, false, priv_data);

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
            return send_last_command();
        }
        default: { return 0; }
        }

        // step 6. setup log & signal
        setup_log();
        setup_signal();
        setup_atbus();

        // TODO step 7. all modules init

        // TODO step 8. all modules reload

        // TODO step 9. set running
        return run_ev_loop(ev_loop);
    }

    int app::reload() {
        // TODO step 1. reset configure
        // TODO step 2. reload from program configure file
        // TODO step 3. reload from external configure files
        // TODO step 4. merge configure

        // step 5. if not in start mode, return
        if (mode_t::START != mode_) {
            return 0;
        }

        // TODO step 6. reset log
        // TODO step 7. if inited, let all modules reload

        // TODO step 8. if running and tick interval changed, reset timer

        return 0;
    }

    int app::stop() {
        // step 1. set stop flag.
        bool is_stoping = set_flag(flag_t::STOPING, true);

        // step 2. stop libuv and return from uv_run
        if (!is_stoping) {
            uv_stop(bus_node_.get_evloop());
        }
        return 0;
    }

    int app::tick() {
        // TODO step 1. proc atbus
        // TODO step 2. proc available modules
        return 0;
    }

    bool app::check(flag_t::type f) const {
        if (f < 0 || f >= flag_t::FLAG_MAX) {
            return false;
        }

        return flags_.test(f);
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
        // TODO setup tick timer
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
        opt_mgr->bind_cmd("-h, --help", &app::prog_option_handler_help, this, &opt_mgr)
            ->set_help_msg("-h. --help                             show this help message.");

        // show version and exit
        opt_mgr->bind_cmd("-v, --version", &app::prog_option_handler_version, this)
            ->set_help_msg("-v, --version                          show version and exit.");

        // set app bus id
        opt_mgr->bind_cmd("-id", &app::prog_option_handler_set_id, this)
            ->set_help_msg("-id <bus id>                           set app bus id.");

        // set configure file path
        opt_mgr->bind_cmd("-c, --conf, --config", &app::prog_option_handler_set_conf_file, this)
            ->set_help_msg("-c, --conf, --config <file path>       set configure file path.");

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

    int app::command_handler_invalid(util::cli::callback_param params) { WLOGERROR("invalid command %s", par.get("@Cmd")->to_string()); }

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
}
