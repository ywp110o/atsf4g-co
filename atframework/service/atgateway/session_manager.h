#ifndef _ATFRAME_SERVICE_ATGATEWAY_SESSION_MANAGER_H_
#define _ATFRAME_SERVICE_ATGATEWAY_SESSION_MANAGER_H_

#pragma once

#include "session.h"
#include <map>

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

        public:
            int init(uv_loop_t *evloop);
            int listen(const char *address);
            int reset();

            inline void *get_private_data() const { return private_data_; }
            inline void set_private_data(void *priv_data) { private_data_ = priv_data; }

        private:
            void on_evt_read_alloc(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf);
            void on_evt_read_data(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf);
            void on_evt_write(uv_write_t *req, int status);
            void on_evt_connected(uv_connect_t *req, int status);
            void on_evt_accept(uv_stream_t *server, int status);
            void on_evt_shutdown(uv_shutdown_t *req, int status);
            void on_evt_closed(uv_handle_t *handle);

        private:
            uv_loop_t *evloop_;
            ATFRAME_GATEWAY_AUTO_MAP(session::id_t, session::ptr_t) sessions_;
            void *private_data_;
        }
    }
}

#endif