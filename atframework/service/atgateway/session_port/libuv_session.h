#ifndef _ATFRAME_SERVICE_ATGATEWAY_LIBUV_SESSION_H_
#define _ATFRAME_SERVICE_ATGATEWAY_LIBUV_SESSION_H_

#pragma once

#ifdef NETWORK_EVPOLL_ENABLE_LIBUV

#include <cstddef>
#include <stdint.h>

#include "uv.h"

#include <std/smart_ptr.h>

#include "detail/buffer.h"
#include "session_base.h"


namespace atframe {
    namespace gateway {
        class libuv_session : public session_base {
        public:
            typedef session_base::id_t id_t;
            typedef session_base::limit_t limit_t;
            typedef std::shared_ptr<libuv_session> ptr_t;

        public:
            static ptr_t create();

        private:
            union {
                uv_tcp_t tcp_handle;
                uv_pipe_t unix_handle;
                uv_udp_t udp_handle;
            };
        };
    }
}

#endif

#endif