
#include "session.h"

namespace atframe {
    namespace gateway {
        session::session() : id_(0), router_(0), flags_(0), private_data_(NULL) { memset(&limit_, 0, sizeof(limit_)); }
        session::~session() { assert(check_flag(flag_t::EN_FT_CLOSING)); }

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

                // TODO shutdown and close uv_stream_t
            }
        }

        bool session::check_flag(flag_t::type t) const { return 0 != (flags_ & t); }

        void session::set_flag(flag_t::type t, bool v) {
            if (v) {
                flags_ |= t;
            } else {
                flags_ &= ~t;
            }
        }

        int session::init_new_session(::atbus::node::id_t router, std::unique_ptr<proto_base> &proto) {
            // TODO alloc id
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

            // TODO send new msg
            ::atframe::gw::ss_msg msg;

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

            // TODO send remove msg
            ::atframe::gw::ss_msg msg;

            int ret = send_to_server(msg);
            if (0 == ret) {
                set_flag(flag_t::EN_FT_REGISTERED, false);
            }

            return ret;
        }
    }
}