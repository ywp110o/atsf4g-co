#include "session_manager.h"

namespace atframe {
    namespace gateway {
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
            // TODO make_address
            // TODO libuv listen and setup callbacks
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

            // TODO close all listen socks
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

        void session_manager::on_evt_read_alloc(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
            // TODO alloc read buffer from session proto

            io_stream_connection *conn_raw_ptr = reinterpret_cast<io_stream_connection *>(handle->data);
            assert(conn_raw_ptr);
            assert(conn_raw_ptr->channel);

            io_stream_flag_guard flag_guard(conn_raw_ptr->channel->flags, io_stream_channel::EN_CF_IN_CALLBACK);

            // 如果正处于关闭阶段，忽略所有数据
            if (io_stream_connection::EN_ST_CONNECTED != conn_raw_ptr->status) {
                buf->base = NULL;
                buf->len = 0;
                uv_read_stop(conn_raw_ptr->handle.get());
            }

            void *data = NULL;
            size_t sread = 0, swrite = 0;
            conn_raw_ptr->read_buffers.back(data, sread, swrite);

            // 正在读取vint时，指定缓冲区为head内存块
            if (NULL == data || 0 == swrite) {
                buf->len = sizeof(conn_raw_ptr->read_head.buffer) - conn_raw_ptr->read_head.len;

                if (0 == buf->len) {
                    // 理论上这里不会走到，因为如果必然会先收取一次header的大小，这时候已经可以解出msg的大小
                    // 如果msg超过限制大小并低于缓冲区大小，则会发出大小错误回调并会减少header的占用量，
                    // 那么下一次这个回调函数调用时buf->len必然大于0
                    // 如果msg超过缓冲区大小，则会出错回调并立即断开连接,不会再有下一次调用
                    buf->base = NULL;
                } else {
                    buf->base = &conn_raw_ptr->read_head.buffer[conn_raw_ptr->read_head.len];
                }
                return;
            }

            // 否则指定为大内存块缓冲区
            buf->base = reinterpret_cast<char *>(data);
            buf->len = swrite;
        }

        void session_manager::on_evt_read_data(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
            session *sess = reinterpret_cast<session *>(stream->data);
            assert(sess);

            io_stream_flag_guard flag_guard(channel->flags, io_stream_channel::EN_CF_IN_CALLBACK);

            // 如果正处于关闭阶段，忽略所有数据
            if (io_stream_connection::EN_ST_CONNECTED != conn_raw_ptr->status) {
                return;
            }

            // if no more data or EAGAIN or break by signal, just ignore
            if (0 == nread || UV_EAGAIN == nread || UV_EAI_AGAIN == nread || UV_EINTR == nread) {
                return;
            }

            // TODO if network error or reset by peer, move session into reconnect queue
            if (nread < 0) {
                session::ptr_t s = sess->shared_from_this();
                channel->error_code = static_cast<int>(nread);
                s->close(close_reason_t::EN_CRT_LOGOUT);

                //
                reconnect_timeout_.push_back();
                reconnect_cache_[sess->get_id()] = s;
                return;
            }

            // TODO alloc call read API of session proto
        }

        void session_manager::on_evt_write(uv_write_t *req, int status) {
            // TODO notify session proto that this write finished
        }

        void session_manager::on_evt_accept(uv_stream_t *server, int status) {
            // TODO create proto object and session object
            // TODO check session number limit
        }

        void session_manager::on_evt_shutdown(uv_shutdown_t *req, int status) {
            // TODO call close API
        }

        void session_manager::on_evt_closed(uv_handle_t *handle) {
            // TODO free session object
        }
    }
}