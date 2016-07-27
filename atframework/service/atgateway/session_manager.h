#ifndef _ATFRAME_SERVICE_ATGATEWAY_SESSION_MANAGER_H_
#define _ATFRAME_SERVICE_ATGATEWAY_SESSION_MANAGER_H_

#pragma once

#include "session.h"
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

                size_t max_client_number;
            };

            struct lister_conf_t {
                std::vector<std::string> address;
                std::string type;
                int backlog;
            };

            struct conf_t {
                client_limit_t limits;
                lister_conf_t listen;
                time_t reconnect_timeout;
                size_t send_buffer_size;
                ::atbus::node::id_t default_router;
            };

            typedef ATFRAME_GATEWAY_AUTO_MAP(session::id_t, session::ptr_t) session_map_t;
            typedef std::function<std::unique_ptr< ::atframe::gateway::proto_base>()> create_proto_fn_t;
            typedef std::function<void(session &)> on_create_session_fn_t;

        public:
            int init(::atbus::node *bus_node, create_proto_fn_t fn);
            /**
             * @brief listen all address in configure
             * @return the number of listened address
             */
            int listen_all();
            int listen(const char *address);
            int reset();
            int tick();
            int close(session::id_t sess_id, int reason);

            inline void *get_private_data() const { return private_data_; }
            inline void set_private_data(void *priv_data) { private_data_ = priv_data; }

            int post_data(bus_id_t tid, int type, const void *buffer, size_t s);

            int push_data(session::id_t sess_id, const void *buffer, size_t s);
            int broadcast_data(const void *buffer, size_t s);

            inline conf_t &get_conf() { return conf_; }
            inline const conf_t &get_conf() const { return conf_; }

            inline on_create_session_fn_t get_on_create_session() const { return on_create_session_fn_; }
            inline void set_on_create_session(on_create_session_fn_t fn) { on_create_session_fn_ = fn; }

        private:
            static void on_evt_read_alloc(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf);
            static void on_evt_read_data(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf);
            static void on_evt_write(uv_write_t *req, int status);
            static void on_evt_accept_tcp(uv_stream_t *server, int status);
            static void on_evt_accept_pipe(uv_stream_t *server, int status);

            static void on_evt_listen_closed(uv_handle_t *handle);

        private:
            struct session_timeout_t {
                time_t timeout;
                session::ptr_t s;
            };

            uv_loop_t *evloop_;
            ::atbus::node *app_node_;
            conf_t conf_;

            create_proto_fn_t create_proto_fn_;
            on_create_session_fn_t on_create_session_fn_;

            typedef std::shared_ptr<uv_stream_t> listen_handle_ptr_t;
            std::list<listen_handle_ptr_t> listen_handles_;
            session_map_t actived_sessions_;
            std::list<session_timeout_t> first_idle_;
            session_map_t reconnect_cache_;
            std::list<session_timeout_t> reconnect_timeout_;
            void *private_data_;
        }
    }
}

#endif