#ifndef _ATFRAME_SERVICE_ATGATEWAY_PROTOCOL_INNER_V1_H_
#define _ATFRAME_SERVICE_ATGATEWAY_PROTOCOL_INNER_V1_H_

#pragma once

#include "detail/buffer.h"

#include "../proto_base.h"

#ifndef ATFRAME_GATEWAY_MACRO_DATA_SMALL_SIZE
#define ATFRAME_GATEWAY_MACRO_DATA_SMALL_SIZE 3072
#endif

namespace atframe {
    namespace gateway {
        class libatgw_proto_inner_v1 : public proto_base {
        private:
            struct recv_cache_t {
                char small_recv_buffer_[ATFRAME_GATEWAY_MACRO_DATA_SMALL_SIZE];
            } recv_cache_;
            ::atbus::detail::buffer_manager send_queue_;
        };
    }
}

#endif