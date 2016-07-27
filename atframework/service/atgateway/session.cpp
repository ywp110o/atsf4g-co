
#include "session.h"

namespace atframe {
    namespace gateway {
        session::session() : id_(0), router_(0), owner_(NULL), flags_(0), peer_port_(0), private_data_(NULL) {
            memset(&limit_, 0, sizeof(limit_));
            raw_handle_.data = this;
        }

        session::~session() { assert(check_flag(flag_t::EN_FT_CLOSING)); }

        bool session::check_flag(flag_t::type t) const { return 0 != (flags_ & t); }

        void session::set_flag(flag_t::type t, bool v) {
            if (v) {
                flags_ |= t;
            } else {
                flags_ &= ~t;
            }
        }

        session::ptr_t session::create(session_manager *mgr, std::unique_ptr<proto_base> &proto) {
            ptr_t ret = std::make_shared<session>();
            if (!ret) {
                return ret;
            }

            ret->owner_ = mgr;
            ret->proto_.swap(proto);
            return ret;
        }

        int session::accept_tcp(uv_stream_t *server) {
            if (check_flag(flag_t::EN_FT_CLOSING)) {
                WLOGERROR("session 0x%p already closed or is closing, can not accept again", this);
                return error_code_t::EN_ECT_CLOSING;
            }

            if (check_flag(flag_t::EN_FT_HAS_FD)) {
                WLOGERROR("session 0x%p already has fd, can not accept again", this);
                return error_code_t::EN_ECT_ALREADY_HAS_FD;
            }

            int errcode = 0;
            if (0 != (errcode = uv_tcp_init(server->loop, &tcp_handle_))) {
                WLOGERROR("session 0x%p init tcp sock failed, error code: %d", this, errcode);
                return error_code_t::EN_ECT_NETWORK;
            }
            set_flag(flag_t::EN_FT_HAS_FD, true);

            if (0 != (errcode = uv_accept(server, &stream_handle_))) {
                WLOGERROR("session 0x%p accept tcp failed, error code: %d", this, errcode);
                return error_code_t::EN_ECT_NETWORK;
            }

            uv_tcp_nodelay(&tcp_handle_, 1);
            uv_stream_set_blocking(&stream_handle_, 0);

            // get peer ip&port
            sockaddr_storage sock_addr;
            int name_len = sizeof(sock_addr);
            uv_tcp_getpeername(&tcp_handle_, reinterpret_cast<struct sockaddr *>(&sock_addr), &name_len);

            char ip[64] = {0};
            if (sock_addr.sa_family == AF_INET6) {
                sockaddr_in6 *sock_addr_ipv6 = reinterpret_cast<struct sockaddr_in6 *>(&sock_addr);
                uv_ip6_name(&sock_addr_ipv6, ip, sizeof(ip));
                peer_ip_ = ip;
                peer_port_ = static_cast<int32_t>(sock_addr_ipv6->sin6_port);
            } else {
                sockaddr_in6 *sock_addr_ipv4 = reinterpret_cast<struct sockaddr_in *>(&sock_addr);
                uv_ip4_name(&sock_addr_ipv4, ip, sizeof(ip));
                peer_ip_ = ip;
                peer_port_ = static_cast<int32_t>(sock_addr_ipv6->sin_port);
            }

            return 0;
        }

        int session::accept_pipe(uv_stream_t *server) {
            if (check_flag(flag_t::EN_FT_CLOSING)) {
                WLOGERROR("session 0x%p already closed or is closing, can not accept again", this);
                return error_code_t::EN_ECT_CLOSING;
            }

            if (check_flag(flag_t::EN_FT_HAS_FD)) {
                WLOGERROR("session 0x%p already has fd, can not accept again", this);
                return error_code_t::EN_ECT_ALREADY_HAS_FD;
            }

            int errcode = 0;
            if (0 != (errcode = uv_pipe_init(server->loop, &unix_handle_, 1))) {
                WLOGERROR("session 0x%p init unix sock failed, error code: %d", this, errcode);
                return error_code_t::EN_ECT_NETWORK;
            }
            set_flag(flag_t::EN_FT_HAS_FD, true);

            if (0 != (errcode = uv_accept(server, &stream_handle_))) {
                WLOGERROR("session 0x%p accept unix failed, error code: %d", this, errcode);
                return error_code_t::EN_ECT_NETWORK;
            }

            uv_stream_set_blocking(&stream_handle_, 0);

            // get peer path
            char pipe_path[util::file_system::MAX_PATH_LEN] = {0};
            size_t path_len = sizeof(pipe_path);
            uv_pipe_getpeername(&unix_handle_, pipe_path, &path_len);
            peer_ip_.assign(pipe_path, path_len);
            peer_port_ = 0;

            return 0;
        }

        int session::init_new_session(::atbus::node::id_t router, std::unique_ptr<proto_base> &proto) {
            static ::atframe::component::timestamp_id_allocator<id_t> id_alloc;
            // alloc id
            id_ = id_alloc.allocate();
            router_ = router;
            proto_.swap(proto);
            return 0;
        }

        int session::init_reconnect(session &sess) {
            // copy id
            id_ = sess.id_;
            router_ = sess.router_;
            limit_ = sess.limit_;
            proto_.swap(sess.proto_);
            private_data_ = sess.private_data_;

            set_flag(flag_t::EN_FT_REGISTERED, get_flag(flag_t::EN_FT_REGISTERED));
            sess.set_flag(flag_t::EN_FT_RECONNECTED, true);
            return 0;
        }

        int session::send_new_session() {
            if (check_flag(flag_t::EN_FT_REGISTERED)) {
                return 0;
            }

            // send new msg
            ::atframe::gw::ss_msg msg;
            msg.init(ATFRAME_GW_CMD_SESSION_ADD, id_);
            ::atframe::gw::ss_body_session *s = msg.body.make_session(peer_ip_, peer_port_);

            int ret = send_to_server(msg);
            if (0 == ret) {
                set_flag(flag_t::EN_FT_REGISTERED, true);
            }

            return ret;
        }

        int session::send_remove_session() {
            if (!check_flag(flag_t::EN_FT_REGISTERED)) {
                return 0;
            }

            // send remove msg
            ::atframe::gw::ss_msg msg;
            msg.init(ATFRAME_GW_CMD_SESSION_REMOVE, id_);

            int ret = send_to_server(msg);
            if (0 == ret) {
                set_flag(flag_t::EN_FT_REGISTERED, false);
            }

            return ret;
        }

        void session::on_alloc_read(size_t suggested_size, char *&out_buf, size_t &out_len) {
            if (proto_) {
                proto_->alloc_recv_buffer(suggested_size, out_buf, out_len);
            }
        }

        void session::on_read(int ssz, const char *buff, size_t len) {
            if (proto_) {
                proto_->read(ssz, buff, len);
            }
        }

        void session::close(int reason) {
            if (check_flag(flag_t::EN_FT_CLOSING)) {
                return;
            }

            set_flag(flag_t::EN_FT_CLOSING, true);
            if (check_flag(flag_t::EN_FT_REGISTERED) && !check_flag(flag_t::EN_FT_RECONNECTED)) {
                send_remove_session();
            }

            if (check_flag(flag_t::EN_FT_HAS_FD)) {
                if (proto_) {
                    proto_->close(reason);
                }

                // shutdown and close uv_stream_t
                // manager can not be used any more
                owner_ = NULL;
                shutdown_req_.data = new ptr_t(shared_from_this());
                uv_shutdown(&shutdown_req_, &stream_handle_, on_evt_shutdown);
                set_flag(flag_t::EN_FT_HAS_FD, false);
            }
        }

        int session::close_fd() {
            if (check_flag(flag_t::EN_FT_HAS_FD)) {
                if (proto_) {
                    proto_->close(reason);
                }

                // shutdown and close uv_stream_t
                // manager can not be used any more
                owner_ = NULL;
                shutdown_req_.data = new ptr_t(shared_from_this());
                uv_shutdown(&shutdown_req_, &stream_handle_, on_evt_shutdown);
                set_flag(flag_t::EN_FT_HAS_FD, false);
            }

            return 0;
        }

        int session::send_to_client(const void *data, size_t len) {
            // send to proto_
            if (check_flag(flag_t::EN_FT_CLOSING)) {
                WLOGERROR("sesseion %llx is closing, can not send data to client any more", id_);
                return error_code_t::EN_ECT_CLOSING;
            }

            if (!proto_) {
                WLOGERROR("sesseion %llx lost protocol handle when send to client", id_);
                return error_code_t::EN_ECT_BAD_PROTOCOL;
            }

            // TODO send limit
            return proto_->write(data, len);
        }

        int session::send_to_server(::atframe::gw::ss_msg &msg) {
            // send to router_
            if (0 == router_) {
                WLOGERROR("sesseion %llx has not configure router", id_);
                return error_code_t::EN_ECT_INVALID_ROUTER;
            }

            if (NULL == mgr_) {
                WLOGERROR("sesseion %llx has lost manager and can not send ss message any more", id_);
                return error_code_t::EN_ECT_LOST_MANAGER;
            }

            // TODO recv limit

            // send to server with type = ::atframe::component::service_type::EN_ATST_GATEWAY
            std::stringstream ss;
            msgpack::pack(ss, m);
            std::string packed_buffer;
            ss.str().swap(packed_buffer);

            return mgr_->post_data(router_, ::atframe::component::service_type::EN_ATST_GATEWAY, packed_buffer.data(),
                                   packed_buffer.size());
        }

        proto_base *session::get_protocol_handle() { return proto_.get(); }
        const proto_base *session::get_protocol_handle() const { return proto_.get(); }

        void session::session_manager::on_evt_shutdown(uv_shutdown_t *req, int status) {
            // call close API
            session *self = reinterpret_cast<session *>(req->handle->data);
            assert(self);

            uv_close(&self->raw_handle_, on_evt_closed);
        }

        void session::session_manager::on_evt_closed(uv_handle_t *handle) {
            session *self = reinterpret_cast<session *>(handle->data);
            assert(self);

            // free session object
            ptr_t *holder = reinterpret_cast<ptr_t *>(self->shutdown_req_.data);
            assert(holder);
            delete holder;
        }
    }
}