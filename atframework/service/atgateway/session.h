#ifndef _ATFRAME_SERVICE_ATGATEWAY_SESSION_H_
#define _ATFRAME_SERVICE_ATGATEWAY_SESSION_H_

#pragma once

#include <cstddef>
#include <stdint.h>

#include "uv.h"

#include <std/smart_ptr.h>

#include "detail/buffer.h"

namespace atframe {
    namespace gateway {
        class session {
        public:
            struct limit_t {
                size_t total_recv_bytes;
                size_t total_send_bytes;
                size_t hour_recv_bytes;
                size_t hour_send_bytes;
                size_t minute_recv_bytes;
                size_t minute_send_bytes;
            };

            struct router_t {
                ::atbus::node::id_t transfer_to;
            };

            typedef std::shared_ptr<session> ptr_t;
            typedef uint64_t id_t;

        public:
            int tell_message_length(size_t len);

            inline void *get_private_data() const { return private_data_; }
            inline void set_private_data(void *priv_data) { private_data_ = priv_data; }

        private:
            id_t id_;
            router_t router_;

            struct recv_cache_t {
                size_t next_message_length;
                std::unique_ptr<char[]> recv_buffer_;
            } recv_cache_;
            ::atbus::detail::buffer_manager send_queue_;
            limit_t limits_;

            union {
                uv_tcp_t tcp_handle;
                uv_pipe_t unix_handle;
                uv_udp_t udp_handle;
            };
        };
    }
}

#endif