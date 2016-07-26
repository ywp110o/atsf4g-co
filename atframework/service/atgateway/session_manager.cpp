#include "session_manager.h"
#include <new>


namespace atframe {
    namespace gateway {
        namespace detail {
            template <typename T>
            static void session_manager_delete_stream_fn(uv_stream_t *handle) {
                if (NULL == handle) {
                    return;
                }

                T *real_conn = reinterpret_cast<T *>(handle);
                // must be closed
                assert(uv_is_closing(reinterpret_cast<uv_handle_t *>(handle)));
                delete real_conn;
            }

            template <typename T>
            static T *session_manager_make_stream_ptr(std::shared_ptr<uv_stream_t> &res) {
                T *real_conn = new (std::nothrow) T();
                uv_stream_t *stream_conn = reinterpret_cast<uv_stream_t *>(real_conn);
                res = std::shared_ptr<uv_stream_t>(stream_conn, session_manager_delete_stream_fn<T>);
                stream_conn->data = NULL;
                return real_conn;
            }
        }

        int session_manager::init(uv_loop_t *evloop, create_proto_fn_t fn) {
            evloop_ = evloop;
            create_proto_fn_ = fn;
            if (!fn) {
                WLOGERROR("create protocol function is required");
                return -1;
            }
            return 0;
        }

        int session_manager::listen(const char *address) {
            // make_address
            ::atbus::channel::channel_address_t addr;
            ::atbus::detail::make_address(address, addr);

            listen_handle_ptr_t res;
            // libuv listen and setup callbacks
            if (0 == UTIL_STRFUNC_STRNCASE_CMP("ipv4:", addr.scheme.c_str(), 5) ||
                0 == UTIL_STRFUNC_STRNCASE_CMP("ipv6:", addr.scheme.c_str(), 5)) {
                uv_tcp_t *tcp_handle = ::atframe::gateway::detail::session_manager_make_stream_ptr<uv_tcp_t>(res);
                if (res) {
                    uv_stream_set_blocking(res.get(), 0);
                    uv_tcp_nodelay(tcp_handle, 1);
                } else {
                    WLOGERROR("create uv_tcp_t failed.");
                    return error_code_t::EN_ECT_NETWORK;
                }

                if (0 != uv_tcp_init(evloop_, tcp_handle)) {
                    WLOGERROR("init listen to %s failed", address);
                    return error_code_t::EN_ECT_NETWORK;
                }

                if ('4' == addr.scheme[3]) {
                    sockaddr_in sock_addr;
                    uv_ip4_addr(addr.host.c_str(), addr.port, &sock_addr);
                    if (0 != uv_tcp_bind(tcp_handle, reinterpret_cast<const sockaddr *>(&sock_addr), 0)) {
                        WLOGERROR("bind sock to %s failed", address);
                        return error_code_t::EN_ECT_NETWORK;
                    }

                    if (0 != uv_listen(res.get(), conf_.backlog, on_evt_accept_tcp)) {
                        WLOGERROR("listen to %s failed", address);
                        return error_code_t::EN_ECT_NETWORK;
                    }

                    tcp_handle->data = this;
                } else {
                    sockaddr_in6 sock_addr;
                    uv_ip6_addr(addr.host.c_str(), addr.port, &sock_addr);
                    if (0 != uv_tcp_bind(tcp_handle, reinterpret_cast<const sockaddr *>(&sock_addr), 0)) {
                        WLOGERROR("bind sock to %s failed", address);
                        return error_code_t::EN_ECT_NETWORK;
                    }

                    if (0 != uv_listen(res.get(), conf_.backlog, on_evt_accept_tcp)) {
                        WLOGERROR("listen to %s failed", address);
                        return error_code_t::EN_ECT_NETWORK;
                    }

                    tcp_handle->data = this;
                }

            } else if (0 == UTIL_STRFUNC_STRNCASE_CMP("unix:", addr.scheme.c_str(), 5)) {
                uv_pipe_t *pipe_handle = ::atframe::gateway::detail::session_manager_make_stream_ptr<uv_pipe_t>(res);
                if (res) {
                    uv_stream_set_blocking(res.get(), 0);
                } else {
                    WLOGERROR("create uv_pipe_t failed.");
                    return error_code_t::EN_ECT_NETWORK;
                }

                if (0 != uv_pipe_init(evloop_, pipe_handle, 1)) {
                    WLOGERROR("init listen to %s failed", address);
                    return error_code_t::EN_ECT_NETWORK;
                }

                if (0 != uv_pipe_bind(pipe_handle, addr.host.c_str())) {
                    WLOGERROR("bind pipe to %s failed", address);
                    return error_code_t::EN_ECT_NETWORK;
                }

                if (0 != uv_listen(res.get(), conf_.backlog, on_evt_accept_pipe)) {
                    WLOGERROR("listen to %s failed", address);
                    return error_code_t::EN_ECT_NETWORK;
                }

                pipe_handle->data = this;
            } else {
                return error_code_t::EN_ECT_INVALID_ADDRESS;
            }

            if (res) {
                listen_handles_.push_back(res);
            }
            return 0;
        }

        int session_manager::reset() {
            // close all sessions
            for (session_map_t::iterator iter = actived_sessions_.begin(); iter != actived_sessions_.end(); ++iter) {
                if (iter->second) {
                    iter->second->close(close_reason_t::EN_CRT_SERVER_CLOSED);
                }
            }
            actived_sessions_.clear();

            for (std::list<session_timeout_t>::iterator iter = first_idle_.begin(); iter != first_idle_.end(); ++iter) {
                if (iter->second) {
                    iter->second->close(close_reason_t::EN_CRT_SERVER_CLOSED);
                }
            }
            first_idle_.clear();

            for (session_map_t::iterator iter = reconnect_cache_.begin(); iter != reconnect_cache_.end(); ++iter) {
                if (iter->second) {
                    iter->second->close(close_reason_t::EN_CRT_SERVER_CLOSED);
                }
            }
            reconnect_cache_.clear();

            for (std::list<session_timeout_t>::iterator iter = reconnect_timeout_.begin(); iter != reconnect_timeout_.end(); ++iter) {
                if (iter->second) {
                    iter->second->close(close_reason_t::EN_CRT_SERVER_CLOSED);
                }
            }
            reconnect_timeout_.clear();

            // close all listen socks
            for (std::list<listen_handle_ptr_t>::iterator iter = listen_handles_.begin(); iter != listen_handles_.end(); ++iter) {
                if (*iter) {
                    // ref count + 1
                    (*iter)->data = new listen_handle_ptr_t(*iter);
                    uv_close(reinterpret_cast<uv_handle_t *>((*iter).get()), on_evt_listen_closed);
                }
            }
            listen_handles_.clear();
            return 0;
        }

        int session_manager::tick() {
            time_t now = util::time::time_utility::get_now();

            // reconnect timeout
            while (!reconnect_timeout_.empty()) {
                if (reconnect_timeout_.front().timeout > now) {
                    break;
                }

                if (reconnect_timeout_.front().s) {
                    session::ptr_t s = reconnect_timeout_.front().s;
                    reconnect_cache_.erase(s->get_id());
                    s->close(close_reason_t::EN_CRT_LOGOUT);
                }
                reconnect_timeout_.pop_front();
            }

            // first idle timeout
            while (!first_idle_.empty()) {
                if (first_idle_.front().timeout > now) {
                    break;
                }

                if (first_idle_.front().s) {
                    session::ptr_t s = first_idle_.front().s;
                    s->close(close_reason_t::EN_CRT_FIRST_IDLE);
                }
                first_idle_.pop_front();
            }

            return 0;
        }

        int session_manager::close(session::id_t sess_id, int reason) {
            session_map_t：：iterator iter = actived_sessions_.find(sess_id);
            if (actived_sessions_.end() != iter) {
                iter->second->close(reason);

                // masual closed sessions will not be moved to reconnect list
                actived_sessions_.erase(iter);
            }

            return 0;
        }

        int session_manager::post_data(bus_id_t tid, int type, const void *buffer, size_t s) {
            // send to process
            if (!post_data_fn_) {
                return error_code_t::EN_ECT_HANDLE_NOT_FOUND;
            }

            return post_data_fn_(tid, type, buffer, s);
        }

        void session_manager::on_evt_read_alloc(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
            // alloc read buffer from session proto
            session *sess = reinterpret_cast<session *>(handle->data);
            assert(sess);
            sess->on_alloc_read(suggested_size, buf->base, buf->len);
        }

        void session_manager::on_evt_read_data(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
            session *sess = reinterpret_cast<session *>(stream->data);
            assert(sess);
            sess->on_read(static_cast<int>(nread), buf->base, buf->len);
        }

        void session_manager::on_evt_write(uv_write_t *req, int status) {
            // TODO notify session proto that this write finished
        }

        void session_manager::on_evt_accept_tcp(uv_stream_t *server, int status) {
            if (0 != status) {
                WLOGERROR("accept tcp socket failed, status: %d", status);
                return;
            }

            // server's data is session_manager
            session_manager *mgr = reinterpret_cast<session_manager *>(server->data);
            assert(mgr);
            std::unique_ptr< ::atframe::gateway::proto_base> proto;
            if (mgr->create_proto_fn_) {
                mgr->create_proto_fn_().swap(proto);
            }

            session::ptr_t sess;
            // create proto object and session object
            if (proto) {
                sess = session::create(mgr, proto);
            }

            if (!sess) {
                WLOGERROR("create proto fn is null or create proto object failed or create session failed");
                listen_handle_ptr_t sp;
                uv_tcp_t *sock = detail::session_manager_make_stream_ptr<uv_tcp_t>(sp);
                if (NULL != sock) {
                    uv_tcp_init(server->loop, sock);
                    uv_accept(server, reinterpret_cast<uv_stream_t *>(sock));
                    sock->data = new listen_handle_ptr_t(sp);
                    uv_close(reinterpret_cast<uv_handle_t *>(sock), on_evt_listen_closed);
                }
                return;
            }

            // create proto object and session object
            int res = sess->accept_tcp(server);
            if (0 != res) {
                sess->close(close_reason_t::EN_CRT_SERVER_BUSY);
            }

            // check session number limit
            if (mgr->conf_.limits.max_client_number > 0 &&
                mgr->actived_sessions_.size() + mgr->actived_sessions_.size() >= mgr->conf_.limits.max_client_number) {
                sess->close(close_reason_t::EN_CRT_SERVER_BUSY);
            }
        }

        void session_manager::on_evt_accept_pipe(uv_stream_t *server, int status) {
            if (0 != status) {
                WLOGERROR("accept tcp socket failed, status: %d", status);
                return;
            }

            // server's data is session_manager
            session_manager *mgr = reinterpret_cast<session_manager *>(server->data);
            assert(mgr);
            std::unique_ptr< ::atframe::gateway::proto_base> proto;
            if (mgr->create_proto_fn_) {
                mgr->create_proto_fn_().swap(proto);
            }

            session::ptr_t sess;
            // create proto object and session object
            if (proto) {
                sess = session::create(mgr, proto);
            }

            if (!sess) {
                WLOGERROR("create proto fn is null or create proto object failed or create session failed");
                listen_handle_ptr_t sp;
                uv_pipe_t *sock = detail::session_manager_make_stream_ptr<uv_pipe_t>(sp);
                if (NULL != sock) {
                    uv_pipe_init(server->loop, sock, 1);
                    uv_accept(server, reinterpret_cast<uv_stream_t *>(sock));
                    sock->data = new listen_handle_ptr_t(sp);
                    uv_close(reinterpret_cast<uv_handle_t *>(sock), on_evt_listen_closed);
                }
                return;
            }

            int res = sess->accept_pipe(server);
            if (0 != res) {
                sess->close(close_reason_t::EN_CRT_SERVER_BUSY);
            }

            // check session number limit
            if (mgr->conf_.limits.max_client_number > 0 &&
                mgr->actived_sessions_.size() + mgr->actived_sessions_.size() >= mgr->conf_.limits.max_client_number) {
                sess->close(close_reason_t::EN_CRT_SERVER_BUSY);
            }
        }

        void session_manager::on_evt_listen_closed(uv_handle_t *handle) {
            // delete shared ptr
            listen_handle_ptr_t *ptr = reinterpret_cast<listen_handle_ptr_t *>(handle->data);
            delete ptr;
        }
    }
}