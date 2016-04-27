/**
 * atapp.h
 *
 *  Created on: 2016年04月23日
 *      Author: owent
 */
#ifndef LIBATAPP_ATAPP_H_
#define LIBATAPP_ATAPP_H_

#pragma once

#include <string>
#include <vector>

#include "std/functional.h"

#include "ini_loader.h"
#include <bitset>

#include "cli/cmd_option.h"

#include "libatbus.h"

#define LIBATAPP_VERSION "0.1.0.0"

namespace atapp {
    class app {
    public:
        typedef atbus::node::bus_id_t app_id_t;
        typedef atbus::protocol::msg_head msg_head_t;

        struct flag_t {
            enum type { RUNNING = 0, STOPING, FLAG_MAX };
        };

        struct mode_t {
            enum type {
                CUSTOM = 0, // custom command
                START,      // start server
                STOP,       // send a stop command
                RELOAD,     // send a reload command
                INFO,       // show information and exit
                MODE_MAX
            };
        };

        // return > 0 means busy and will enter tick again as soon as possiable
        typedef std::function<int()> tick_handler_t;
        // parameters is (message head, buffer address, buffer size)
        typedef std::function<int(const msg_head_t *, const void *, size_t)> msg_handler_t;

    public:
        app();
        ~app();

        int run(atbus::adapter::loop_t *ev_loop, int argc, const char *argv[], void *priv_data = NULL);

        int reload();

        int stop();

        int tick();

        bool check(flag_t::type f) const;

        // api: add custom log type
        // api: add custom module
        // api: add custom command callback
        util::cli::cmd_option_ci::ptr_type get_command_manager();

        // api: add custem program options
        util::cli::cmd_option::ptr_type get_option_manager();

        void set_app_version(const std::string &ver);

        const std::string &get_app_version() const;

    private:
        bool set_flag(flag_t::type f, bool v);

        int run_ev_loop(atbus::adapter::loop_t *ev_loop);

        void setup_signal();

        void setup_option(int argc, const char *argv[], void *priv_data);

        void setup_command();

        void setup_log();

        void setup_atbus();

        int send_last_command();

        // ============ inner functional handlers ============
    private:
        int prog_option_handler_help(util::cli::callback_param params, util::cli::cmd_option *opt_mgr);
        int prog_option_handler_version(util::cli::callback_param params);
        int prog_option_handler_set_id(util::cli::callback_param params);
        int prog_option_handler_set_conf_file(util::cli::callback_param params);
        int prog_option_handler_start(util::cli::callback_param params);
        int prog_option_handler_stop(util::cli::callback_param params);
        int prog_option_handler_reload(util::cli::callback_param params);
        int prog_option_handler_run(util::cli::callback_param params);

        int command_handler_start(util::cli::callback_param params);
        int command_handler_stop(util::cli::callback_param params);
        int command_handler_reload(util::cli::callback_param params);
        int command_handler_invalid(util::cli::callback_param params);

    private:
        static app *last_instance_;
        util::config::ini_loader cfg_loader_;
        util::cli::cmd_option::ptr_type app_option_;
        util::cli::cmd_option_ci::ptr_type cmd_handler_;
        std::vector<std::string> last_command_;

        app_conf conf_;

        atbus::node bus_node_;
        std::bitset<flag_t::FLAG_MAX> flags_;
        mode_t::type mode_;
    };
}

#endif
