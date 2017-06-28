//
// Created by owt50 on 2016/9/28.
//

#ifndef _RPC_DB_UNPACK_H
#define _RPC_DB_UNPACK_H

#pragma once

#include <vector>

namespace hello {
    class message_container;
}

extern "C" struct redisReply;

namespace rpc {
    namespace db {
        namespace detail {

            int32_t do_nothing(hello::message_container& msgc, const redisReply* data);

            int32_t unpack_integer(hello::message_container& msgc, const redisReply* data);

            int32_t unpack_str(hello::message_container& msgc, const redisReply* data);

            int32_t unpack_arr_str(hello::message_container& msgc, const redisReply* data);
        }
    }
}

#endif //_RPC_DB_UNPACK_H
