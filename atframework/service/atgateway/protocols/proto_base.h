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
            };
        };

        struct close_reason_t {
            enum type {
                EN_CRT_LOGOUT = 0,
                EN_CRT_FIRST_IDLE,
                EN_CRT_SERVER_CLOSED,
            };
        };

        class proto_base {
        public:
            typedef std::function<int(proto_base *, const void *, size_t, bool &)> on_write_start_fn_t;
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
                    EN_FT_CLOSED = 0x0004,
                };
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
            virtual void alloc_recv_buffer(size_t suggested_size, char *&out_buf, size_t &out_len) = 0;
            virtual void read(int ssz, const char *buff, size_t len) = 0;

            virtual int write(const void *buffer, size_t len) = 0;
            virtual int write_done(int status);

            virtual int close(int reason);

        protected:
            static void *get_tls_buffer(tls_buffer_t::type tls_type);

        public:
            inline proto_callbacks_t *get_callbacks() const { return callbacks_; }
            inline void set_callbacks(proto_callbacks_t *callbacks) { callbacks_ = callbacks; }

            inline void *get_private_data() const { return private_data_; }
            inline void set_private_data(void *priv_data) { private_data_ = priv_data; }

        protected:
            int flags_;

            proto_callbacks_t *callbacks_;
            void *private_data_;
        };
    }
}

#endif