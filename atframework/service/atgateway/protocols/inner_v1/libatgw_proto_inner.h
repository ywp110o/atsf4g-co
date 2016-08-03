#ifndef _ATFRAME_SERVICE_ATGATEWAY_PROTOCOL_INNER_V1_H_
#define _ATFRAME_SERVICE_ATGATEWAY_PROTOCOL_INNER_V1_H_

#pragma once

#include "detail/buffer.h"

#include "../proto_base.h"

#ifndef ATFRAME_GATEWAY_MACRO_DATA_SMALL_SIZE
#define ATFRAME_GATEWAY_MACRO_DATA_SMALL_SIZE 3072
#endif

namespace atframe {
    namespace gateway {
        class libatgw_proto_inner_v1 : public proto_base {
        public:
            /**
             * @brief crypt configure
             * @note default reuse the definition of inner ptotocol, if it's useful for other protocol depends other protocol's implement
             * @see protocols/inner_v1/libatgw_proto_inner.fbs
             */
            struct crypt_conf_t {
                std::string default_key; /** default key, different used for different crypt protocol **/
                time_t update_interval;  /** crypt key refresh interval **/
                int type;                /** crypt type. XTEA, AES and etc. **/
                int switch_secret_type;  /** how to generate the secret key, dh, rsa or direct send. recommander to use DH **/
                uint32_t keybits;        /** key length in bits. **/

                int rsa_sign_type;           /** RSA sign type. PKCS1, PKCS1_V15 or PSS **/
                int hash_id;                 /** hash id, md5,sha1,sha256,sha512 **/
                std::string rsa_public_key;  /** RSA public key file path. **/
                std::string rsa_private_key; /** RSA private key file path. **/
                std::string dh_param;        /** DH parameter file path. **/
            };

            struct crypt_session_t {
                int type;           /** crypt type. XTEA, AES and etc. **/
                std::string secret; /** crypt secret. **/
                uint32_t keybits;   /** key length in bits. **/

                std::string param; /** cache data used for generate key, dhparam if using DH algorithm. **/
            };

        public:
            libatgw_proto_inner_v1();
            virtual ~libatgw_proto_inner_v1();

            virtual void alloc_recv_buffer(size_t suggested_size, char *&out_buf, size_t &out_len) = 0;
            virtual void read(int ssz, const char *buff, size_t len, int &errcode);

            void dispatch_data(const char *buff, size_t len, int errcode);

            int try_write();
            int write_msg(flatbuffers::FlatBufferBuilder &builder);
            virtual int write(const void *buffer, size_t len);
            virtual int write_done(int status);

            virtual int close(int reason);
            int close(int reason, bool is_send_kickoff);

            virtual bool check_reconnect(proto_base *other);

            virtual void set_recv_buffer_limit(size_t max_size, size_t max_number);
            virtual void set_send_buffer_limit(size_t max_size, size_t max_number);

            static int global_reload(crypt_conf_t &crypt_conf);

            int send_ping();
            int send_pong(time_t tp);
            int send_key_syn(const std::string &secret);
            int send_key_ack(const std::string &secret);
            int send_kickoff(int reason);

        private:
            ::atbus::detail::buffer_manager read_buffers_;
            /**
            * @brief 由于大多数数据包都比较小
            *        当数据包比较小时和动态直接放在动态int的数据包一起，这样可以减少内存拷贝次数
            */
            typedef struct {
                char buffer[ATFRAME_GATEWAY_MACRO_DATA_SMALL_SIZE]; // 小数据包存储区
                size_t len;                                         // 小数据包存储区已使用长度
            } read_head_t;
            read_head_t read_head_;

            ::atbus::detail::buffer_manager write_buffers_;
            const void *last_write_ptr_;
            int close_reason_;

            // crypt option
            crypt_session_t crypt_info_;
        };
    }
}

#endif