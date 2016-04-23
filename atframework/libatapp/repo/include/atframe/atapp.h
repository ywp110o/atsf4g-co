/**
 * atapp.h
 *
 *  Created on: 2016年04月23日
 *      Author: owent
 */
#ifndef LIBATAPP_ATAPP_H_
#define LIBATAPP_ATAPP_H_

#pragma once

#include "ini_loader.h"
#include <bitset>

#include "cli/cmd_option.h"

#include "libatbus.h"

namespace atapp {
    class app {
    public:
        struct flag_t {
            enum type { RUNNING = 0, STOPING, FLAG_MAX };
        };

    public:
        app();
        ~app();

        int run(int argc, const char *argv[], void *priv_data = NULL);

        int reload();

        int stop();

        int proc();

        // api: add custom log type
        // api: add custom module
        // api: add custom cmd callback
        // api: add custem program options
    private:
        int ev_loop();

    private:
        util::config::ini_loader cfg_loader_;
        util::cli::cmd_option cmd_opts_;
        atbus::node bus_node_;
        std::bitset<flag_t::FLAG_MAX> flags_;
    };
}

#endif
