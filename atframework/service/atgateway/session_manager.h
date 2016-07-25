#ifndef _ATFRAME_SERVICE_ATGATEWAY_SESSION_MANAGER_H_
#define _ATFRAME_SERVICE_ATGATEWAY_SESSION_MANAGER_H_

#pragma once

#include "session_port/libuv_session.h"
#include <list>
#include <map>
#include <std/functional.h>

#if (defined(__cplusplus) && __cplusplus >= 201103L) || (defined(_MSC_VER) && _MSC_VER >= 1600)
#include <unordered_map>
#define ATFRAME_GATEWAY_AUTO_MAP(...) std::unordered_map<__VA_ARGS__>

#else
#include <map>
#define ATFRAME_GATEWAY_AUTO_MAP(...) std::map<__VA_ARGS__>
#endif

namespace atframe {
    namespace gateway {
        class session_manager {
        public:
            struct client_limit_t {
                size_t total_recv_limit;
                size_t total_send_limit;
                size_t hour_recv_limit;
                size_t hour_send_limit;
                size_t minute_recv_limit;
                size_t minute_send_limit;

                size_t max_message_size;
            };

            typedef ATFRAME_GATEWAY_AUTO_MAP(session::id_t, session::ptr_t) session_map_t;
            typedef std::function<std::unique_ptr< ::atframe::gateway::proto_base>()> create_proto_fn_t;

        public:
            int init(uv_loop_t *evloop, create_proto_fn_t fn);
            int listen(const char *address);
            int reset();
            int tick();
            int close(session::id_t sess_id, int reason);

            inline void *get_private_data() const { return private_data_; }
            inline void set_private_data(void *priv_data) { private_data_ = priv_data; }

        private:
            void on_evt_read_alloc(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf);
            void on_evt_read_data(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf);
            void on_evt_write(uv_write_t *req, int status);
            void on_evt_accept(uv_stream_t *server, int status);
            void on_evt_shutdown(uv_shutdown_t *req, int status);
            void on_evt_closed(uv_handle_t *handle);

        private:
            struct session_timeout_t {
                time_t timeout;
                session::ptr_t s;
            };

            uv_loop_t *evloop_;
            create_proto_fn_t create_proto_fn_;

            session_map_t actived_sessions_;
            std::list<session_timeout_t> first_idle_;
            session_map_t reconnect_cache_;
            std::list<session_timeout_t> reconnect_timeout_;
            void *private_data_;
        }
    }
}

#endif