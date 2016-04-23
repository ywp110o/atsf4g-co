/**
 * atapp_conf.h
 *
 *  Created on: 2016年04月23日
 *      Author: owent
 */
#ifndef LIBATAPP_ATAPP_H_
#define LIBATAPP_ATAPP_H_

#pragma once

#include "libatbus.h"
#include <string>
#include <time_t>
#include <vector>

namespace atapp {
    struct app_conf {
        // bus configure
        atbus::node::bus_id_t id;
        std::vector<std::string> bus_listen;
        atbus::node::conf_t bus_conf;

        // app configure
        time_t stop_timeout;  // module timeout when receive a stop message
        time_t tick_interval; // tick interval
    };
}

#endif
