
#include <cstddef>
#include <stdint.h>

#include "detail/buffer.h"

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
            typedef uint64_t id_t;

            struct error_code_t {
                enum type {
                    EN_ECT_SUCCESS = 0,
                    EN_ECT_WRITE_BUFFER_NOT_ENOUGH = -1,
                    EN_ECT_READ_BUFFER_NOT_ENOUGH = -2,
                };
            };

            struct flag_t {
                enum type {
                    EN_FT_WRITING = 0x0001,
                    EN_FT_CLOSING = 0x0002,
                    EN_FT_CLOSED = 0x0004,
                };
            };

        protected:
            session_base();
            virtual ~session_base();

        public:
            /**
             * @biref push data to write queue, if session is not writing, will call write()
             */
            virtual int push(const void *data, size_t len);

            /**
             * @biref try to real write data
             */
            virtual int write();

            /**
             * @biref poll once
             */
            virtual int poll();

            /**
             * @biref write data to session
             */
            virtual int on_write(const void *data, size_t len);

            /**
             * @biref read data from session
             */
            virtual int on_read(const void *data, size_t *len);

            /**
             * @biref alloc read buffer
             * @param data output the started free buffer
             * @param len the length need
             */
            virtual int alloc_read_buffer(const void **data, size_t len);

            int tell_message_length(size_t len);

            inline void *get_private_data() const { return private_data_; }
            inline void set_private_data(void *priv_data) { private_data_ = priv_data; }

        private:
            struct recv_cache_t {
                size_t next_message_length;
                std::unique_ptr<char[]> recv_buffer_;
            } recv_cache_;
            ::atbus::detail::buffer_manager send_queue_;
            limit_t limits_;
            int flags_;

            void *private_data_;
        };
    }
}