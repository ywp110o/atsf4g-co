#ifndef _ATFRAME_SERVICE_ATGATEWAY_SESSION_H_
#define _ATFRAME_SERVICE_ATGATEWAY_SESSION_H_

#pragma once

#include <cstddef>
#include <stdint.h>

#include "uv.h"

#include <std/smart_ptr.h>

#include "protocol/libatgw_server_protocol.h"

namespace atframe {
    namespace gateway {
        class session : std::shared_from_this<session> {
        public:
            struct limit_t {
                size_t total_recv_bytes;
                size_t total_send_bytes;
                size_t hour_recv_bytes;
                size_t hour_send_bytes;
                size_t minute_recv_bytes;
                size_t minute_send_bytes;
            };

            typedef uint64_t id_t;

            struct flag_t {
                enum type {
                    EN_FT_INITED = 0x0001,
                    EN_FT_HAS_FD = 0x0002,
                    EN_FT_REGISTERED = 0x0004,
                    EN_FT_RECONNECTED = 0x0008,
                    EN_FT_CLOSING = 0x0010,
                    EN_FT_IN_CALLBACK = 0x0020,
                };
            };

            typedef std::shared_ptr<session> ptr_t;

        public:
            session();
            ~session();

            bool check_flag(flag_t::type t) const;

            void set_flag(flag_t::type t, bool v);

            static ptr_t create();

            inline id_t get_id() const { return id_; };

            int init_new_session(::atbus::node::id_t router, std::unique_ptr<proto_base> &proto);

            int init_reconnect(session &sess);

            int close(int reason);

            int send_to_client(const void *data, size_t len);

            int send_to_server(const ::atframe::gw::ss_msg &msg);

        private:
            int send_new_session();
            int send_remove_session();

        public:
            inline void *get_private_data() const { return private_data_; }
            inline void set_private_data(void *priv_data) { private_data_ = priv_data; }
            inline ::atbus::node::id_t get_router() const { return router_; }
            inline void set_router(::atbus::node::id_t id) { router_ = id; }

        private:
            id_t id_;
            ::atbus::node::id_t router_;
            limit_t limit_;
            int flags_;
            union {
                uv_tcp_t tcp_handle;
                uv_pipe_t unix_handle;
                uv_udp_t udp_handle;
            };

            std::unique_ptr<proto_base> proto_;
            void *private_data_;
        };
    }
}

#endif