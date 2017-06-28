//
// Created by owt50 on 2016/9/29.
//

#ifndef _CONFIG_EXTERN_LOG_CATEGORIZE_H
#define _CONFIG_EXTERN_LOG_CATEGORIZE_H

#include <log/log_wrapper.h>
#include <config/compiler_features.h>

struct log_categorize_t {
    enum type {
        DEFAULT = ::util::log::log_wrapper::categorize_t::DEFAULT,
        DB,
        PROTO_STAT,
        PAY,
        MAX
    };
};

UTIL_CONFIG_STATIC_ASSERT_MSG(((int)log_categorize_t::MAX) <= ((int)::util::log::log_wrapper::categorize_t::MAX), "log categorize is too large");

#endif //ATF4G_CO_EXTERN_LOG_CATEGORIZE_H
