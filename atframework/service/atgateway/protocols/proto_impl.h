#include "session_base.h"

namespace atframe {
    namespace gateway {

        class proto_impl {
        public:
            virtual ~proto_impl() = 0;

            // action handle
            virtual int on_recv_data(session_base *sess, const void *buffer, size_t len) = 0;
            virtual int on_recv_message(session_base *sess, const void *buffer, size_t len);
            virtual int on_send_message(session_base *sess, const void *buffer, size_t len) = 0;
            virtual int on_close_session(session_base *sess, int reason);
            virtual int on_add_session(session_base *sess);
            virtual int on_remove_session(session_base *sess);

            inline void *get_private_data() const { return private_data_; }
            inline void set_private_data(void *priv_data) { private_data_ = priv_data; }

        private:
            void *private_data_;
        };
    }
}