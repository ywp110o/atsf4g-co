
#include <cstddef>
#include <stdint.h>

namespace atframe {
    namespace gateway {
        class session_base {
        public:
            struct limit_t {
                size_t total_recv_bytes;
                size_t total_send_bytes;
                size_t hour_recv_bytes;
                size_t hour_send_bytes;
                size_t minute_recv_bytes;
                size_t minute_send_bytes;
            };


            int tell_message_length(size_t len);

            inline void *get_private_data() const { return private_data_; }
            inline void set_private_data(void *priv_data) { private_data_ = priv_data; }

        private:
            void *private_data_;
        };
    }
}