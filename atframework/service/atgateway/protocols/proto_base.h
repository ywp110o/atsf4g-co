#ifndef _ATFRAME_SERVICE_ATGATEWAY_PROTOCOL_BASE_H_
#define _ATFRAME_SERVICE_ATGATEWAY_PROTOCOL_BASE_H_

#pragma once

#include <cstddef>
#include <std/functional.h>
#include <stdint.h>

namespace atframe {
    namespace gateway {
        struct error_code_t {
            enum type {
                EN_ECT_SUCCESS = 0,
                EN_ECT_FIRST_IDEL = -1001,
                EN_ECT_HANDSHAKE = -1002,
                EN_ECT_BUSY = -1003,
                EN_ECT_SESSION_EXPIRED = -1004,
                EN_ECT_REFUSE_RECONNECT = -1005,
                EN_ECT_MISS_CALLBACKS = -1006,
                EN_ECT_INVALID_ROUTER = -1007,
                EN_ECT_INVALID_ADDRESS = -1008,
                EN_ECT_NETWORK = -1009,
                EN_ECT_BAD_PROTOCOL = -1010,
                EN_ECT_CLOSING = -1011,
                EN_ECT_LOST_MANAGER = -1012,
                EN_ECT_MSG_TOO_LARGE = -1013,
                EN_ECT_HANDLE_NOT_FOUND = -1014,
                EN_ECT_ALREADY_HAS_FD = -1015,
                EN_ECT_SESSION_NOT_FOUND = -1016,
                EN_ECT_NOT_WRITING = -1017,
                EN_ECT_CRYPT_NOT_SUPPORTED = -1018,
                EN_ECT_PARAM = -1019,
                EN_ECT_BAD_DATA = -1020,
                EN_ECT_INVALID_SIZE = -1021,
            };
        };

        struct close_reason_t {
            enum type {
                EN_CRT_LOGOUT = 0,
                EN_CRT_FIRST_IDLE,
                EN_CRT_SERVER_CLOSED,
                EN_CRT_SERVER_BUSY,
                EN_CRT_KICKOFF,
                EN_CRT_TRAFIC_EXTENDED,
                EN_CRT_INVALID_DATA,
            };
        };

        class proto_base {
        public:
            typedef std::function<int(proto_base *, void *, size_t, bool *)> on_write_start_fn_t;
            typedef std::function<int(proto_base *, const void *, size_t)> on_message_fn_t;
            typedef std::function<int(proto_base *, uint64_t &)> on_init_new_session_fn_t;
            typedef std::function<int(proto_base *, uint64_t)> on_init_reconnect_fn_t;
            typedef std::function<int(proto_base *, int)> on_close_fn_t;

            struct tls_buffer_t {
                enum type {
                    EN_TBT_MERGE = 0,
                    EN_TBT_CRYPT,
                    EN_TBT_ZIP,
                    EN_TBT_MAX,
                };
            };

            struct flag_t {
                enum type {
                    EN_PFT_WRITING = 0x0001,
                    EN_FT_CLOSING = 0x0002,
                    EN_FT_IN_CALLBACK = 0x0004,
                };
            };

            struct flag_guard_t {
                int *flags_;
                int v_;
                flag_guard_t(int &f, bool v);
                ~flag_guard_t();

                flag_guard_t(const flag_guard_t &other);
                flag_guard_t &operator=(const flag_guard_t &other);
            };

            struct proto_callbacks_t {
                on_write_start_fn_t write_fn;
                on_message_fn_t message_fn;
                on_init_new_session_fn_t new_session_fn;
                on_init_reconnect_fn_t reconnect_fn;
                on_close_fn_t close_fn;
            };

        protected:
            proto_base();
            virtual ~proto_base() = 0;

        public:
            bool check_flag(flag_t::type t) const;

            void set_flag(flag_t::type t, bool v);

            virtual void alloc_recv_buffer(size_t suggested_size, char *&out_buf, size_t &out_len) = 0;
            virtual void read(int ssz, const char *buff, size_t len, int &errcode) = 0;

            virtual int write(const void *buffer, size_t len) = 0;
            virtual int write_done(int status);

            virtual int close(int reason);

            virtual bool check_reconnect(proto_base *other);

            virtual void set_recv_buffer_limit(size_t max_size, size_t max_number);
            virtual void set_send_buffer_limit(size_t max_size, size_t max_number);

        public:
            static void *get_tls_buffer(tls_buffer_t::type tls_type);
            static size_t get_tls_length(tls_buffer_t::type tls_type);

        public:
            inline proto_callbacks_t *get_callbacks() const { return callbacks_; }
            inline void set_callbacks(proto_callbacks_t *callbacks) { callbacks_ = callbacks; }

            inline void *get_private_data() const { return private_data_; }
            inline void set_private_data(void *priv_data) { private_data_ = priv_data; }

            inline size_t get_write_header_offset() const { return write_header_offset_; }
            inline void set_write_header_offset(size_t sz) {
                if (0 == sz) {
                    write_header_offset_ = sz;
                } else {
                    // padding to sizeof(size_t)
                    write_header_offset_ = (sz + sizeof(size_t) - 1) & (~(sizeof(size_t) - 1));
                }
            }

        protected:
            int flags_;

            /**
             * @brief instead of malloc new data block, we can use some buffer in write buffer to hold the additional data.
             *      For example, we can store uv_write_t at the head of buffer when we use libuv
             */
            size_t write_header_offset_;

            proto_callbacks_t *callbacks_;
            void *private_data_;
        };
    }
}

#endif