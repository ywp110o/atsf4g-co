#include "algorithm/murmur_hash.h"
#include "lock/lock_holder.h"
#include "lock/seq_alloc.h"
#include "lock/spin_lock.h"

#include "libatgw_proto_inner.h"

// the same as openssl, mbedtls also use this as a constant integer
#ifndef AES_BLOCK_SIZE
#define AES_BLOCK_SIZE 16
#endif

#ifndef UTIL_FS_OPEN
#if (defined(_MSC_VER) && _MSC_VER >= 1600) || (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L)
#define UTIL_FS_OPEN(e, f, path, mode) errno_t e = fopen_s(&f, path, mode)
#define UTIL_FS_C11_API
#else
#include <errno.h>
#define UTIL_FS_OPEN(e, f, path, mode) \
    f = fopen(path, mode);             \
    int e = errno
#endif
#endif

#ifndef UNUSED
#define UNUSED(x) ((void)x)
#endif

namespace atframe {
    namespace gateway {
        namespace detail {
            static uint64_t alloc_seq() {
                static ::util::lock::seq_alloc_u64 seq_alloc;
                uint64_t ret = seq_alloc.inc();
                while (0 == ret) {
                    ret = seq_alloc.inc();
                }
                return ret;
            }

            struct crypt_global_configure_t {
                typedef std::shared_ptr<crypt_global_configure_t> ptr_t;

                crypt_global_configure_t(const libatgw_proto_inner_v1::crypt_conf_t &conf) : conf_(conf), inited_(false) {}
                ~crypt_global_configure_t() { close(); }

                int init() {
                    int ret = 0;
                    close();

                    if (::atframe::gw::inner::v1::crypt_type_t_EN_ET_NONE == conf_.type) {
                        return ret;
                    }

                    switch (conf_.switch_secret_type) {
                    case ::atframe::gw::inner::v1::switch_secret_t_EN_SST_DH: {

                        FILE *pem = NULL;
                        UTIL_FS_OPEN(pem_file_e, pem, conf_.dh_param.c_str(), "r");
                        UNUSED(pem_file_e);
                        if (NULL == pem) {
                            ret = error_code_t::EN_ECT_CRYPT_READ_DHPARAM_FILE;
                            break;
                        }
                        fseek(pem, 0, SEEK_END);
                        size_t pem_sz = static_cast<size_t>(ftell(pem));
                        fseek(pem, 0, SEEK_SET);
// init DH param file
#if defined(LIBATFRAME_ATGATEWAY_ENABLE_OPENSSL) || defined(LIBATFRAME_ATGATEWAY_ENABLE_LIBRESSL)
                        openssl_dh_bio_ = NULL;

                        do {
                            unsigned char *pem_buf = reinterpret_cast<unsigned char *>(calloc(pem_sz, sizeof(unsigned char)));
                            if (!pem_buf) {
                                ret = error_code_t::EN_ECT_MALLOC;
                                break;
                            }
                            fread(pem_buf, sizeof(unsigned char), pem_sz, pem);
                            openssl_dh_bio_ = BIO_new_mem_buf(pem_buf, static_cast<int>(pem_sz));
                        } while (false);

                        if (0 == ret) {
                            inited_ = true;
                        } else {
                            if (NULL != openssl_dh_bio_) {
                                BIO_free(openssl_dh_bio_);
                                openssl_dh_bio_ = NULL;
                            }
                        }
// PEM_read_DHparams
#elif defined(LIBATFRAME_ATGATEWAY_ENABLE_MBEDTLS)
                        // mbedtls_dhm_read_params
                        mbedtls_dh_param_.resize(pem_sz * sizeof(unsigned char), 0);
                        fread(&mbedtls_dh_param_[0], sizeof(unsigned char), pem_sz, pem);

                        // test
                        mbedtls_dhm_context test_dh_ctx;
                        mbedtls_dhm_init(&test_dh_ctx);
                        if (0 != mbedtls_dhm_parse_dhm(&test_dh_ctx, reinterpret_cast<const unsigned char *>(mbedtls_dh_param_.data()),
                                                       pem_sz)) {
                            ret = error_code_t::EN_ECT_CRYPT_INIT_DHPARAM;
                        }
                        mbedtls_dhm_free(&test_dh_ctx);

                        if (0 == ret) {
                            inited_ = true;
                        } else {
                            mbedtls_dh_param_.clear();
                        }
#endif

                        if (NULL != pem) {
                            fclose(pem);
                        }
                        break;
                    }
                    case ::atframe::gw::inner::v1::switch_secret_t_EN_SST_RSA: {
                        // TODO init public key and private key
                        // mbedtls: use API in mbedtls/pk.h => mbedtls_pk_parse_public_key, mbedtls_pk_parse_keyfile
                        // openssl/libressl: use API in openssl/rsa.h,openssl/pem.h => RSA * = PEM_read_RSA_PUBKEY, PEM_read_RSAPrivateKey
                        return error_code_t::EN_ECT_CRYPT_NOT_SUPPORTED;
                        break;
                    }
                    case ::atframe::gw::inner::v1::switch_secret_t_EN_SST_DIRECT: {
                        break;
                    }
                    default: {
                        return error_code_t::EN_ECT_CRYPT_NOT_SUPPORTED;
                        break;
                    }
                    }

                    return ret;
                }

                void close() {
                    if (!inited_) {
                        return;
                    }
                    inited_ = false;

                    switch (conf_.switch_secret_type) {
                    case ::atframe::gw::inner::v1::switch_secret_t_EN_SST_DH: {
// free DH param file
#if defined(LIBATFRAME_ATGATEWAY_ENABLE_OPENSSL) || defined(LIBATFRAME_ATGATEWAY_ENABLE_LIBRESSL)
                        if (NULL != openssl_dh_bio_) {
                            BIO_free(openssl_dh_bio_);
                            openssl_dh_bio_ = NULL;
                        }
// PEM_read_DHparams
#elif defined(LIBATFRAME_ATGATEWAY_ENABLE_MBEDTLS)
                        mbedtls_dh_param_.clear();
#endif
                        break;
                    }
                    case ::atframe::gw::inner::v1::switch_secret_t_EN_SST_RSA: {
                        // TODO init public key and private key
                        // mbedtls: use API in mbedtls/pk.h => mbedtls_pk_parse_public_key, mbedtls_pk_parse_keyfile
                        // openssl/libressl: use API in openssl/rsa.h,openssl/pem.h => RSA * = PEM_read_RSA_PUBKEY, PEM_read_RSAPrivateKey
                        // error_code_t::EN_ECT_CRYPT_NOT_SUPPORTED;
                        break;
                    }
                    case ::atframe::gw::inner::v1::switch_secret_t_EN_SST_DIRECT: {
                        break;
                    }
                    default: {
                        // error_code_t::EN_ECT_CRYPT_NOT_SUPPORTED;
                        break;
                    }
                    }
                }

                libatgw_proto_inner_v1::crypt_conf_t conf_;
                bool inited_;

#if defined(LIBATFRAME_ATGATEWAY_ENABLE_OPENSSL) || defined(LIBATFRAME_ATGATEWAY_ENABLE_LIBRESSL)
                BIO *openssl_dh_bio_;
#elif defined(LIBATFRAME_ATGATEWAY_ENABLE_MBEDTLS)
                std::string mbedtls_dh_param_;
#endif

                static ptr_t &current() {
                    static ptr_t ret;
                    return ret;
                }
            };
        }

        libatgw_proto_inner_v1::crypt_session_t::crypt_session_t() : type(::atframe::gw::inner::v1::crypt_type_t_EN_ET_NONE), keybits(0) {}

        libatgw_proto_inner_v1::crypt_session_t::~crypt_session_t() { close(); }

        int libatgw_proto_inner_v1::crypt_session_t::setup(int t, uint32_t kb) {
            if (::atframe::gw::inner::v1::crypt_type_t_EN_ET_NONE != type) {
                return error_code_t::EN_ECT_CRYPT_ALREADY_INITED;
            }

            if (secret.size() * 8 < kb) {
                secret.resize(kb / 8, 0);
            }

            if (secret.size() * 8 > kb) {
                secret.resize(kb / 8);
            }

            switch (t) {
            case ::atframe::gw::inner::v1::crypt_type_t_EN_ET_XXTEA: {
                if (kb != 8 * sizeof(xtea_key.util_xxtea_ctx)) {
                    return error_code_t::EN_ECT_CRYPT_NOT_SUPPORTED;
                }

                ::util::xxtea_setup(&xtea_key.util_xxtea_ctx, reinterpret_cast<const unsigned char *>(secret.c_str()));
                break;
            }
            case ::atframe::gw::inner::v1::crypt_type_t_EN_ET_AES: {
                if (secret.size() * 8 < kb) {
                    return error_code_t::EN_ECT_CRYPT_NOT_SUPPORTED;
                }

#if defined(LIBATFRAME_ATGATEWAY_ENABLE_OPENSSL) || defined(LIBATFRAME_ATGATEWAY_ENABLE_LIBRESSL)
                int res = AES_set_encrypt_key(reinterpret_cast<const unsigned char *>(secret.c_str()), kb, &aes_key.openssl_encrypt_key);
                if (res < 0) {
                    return res;
                }

                res = AES_set_encrypt_key(reinterpret_cast<const unsigned char *>(secret.c_str()), kb, &aes_key.openssl_decrypt_key);
                if (res < 0) {
                    return res;
                }

#elif defined(LIBATFRAME_ATGATEWAY_ENABLE_MBEDTLS)
                mbedtls_aes_init(&aes_key.mbedtls_aes_encrypt_ctx);
                mbedtls_aes_init(&aes_key.mbedtls_aes_decrypt_ctx);
                int res =
                    mbedtls_aes_setkey_enc(&aes_key.mbedtls_aes_encrypt_ctx, reinterpret_cast<const unsigned char *>(secret.c_str()), kb);
                if (res < 0) {
                    return res;
                }

                res = mbedtls_aes_setkey_enc(&aes_key.mbedtls_aes_decrypt_ctx, reinterpret_cast<const unsigned char *>(secret.c_str()), kb);
                if (res < 0) {
                    return res;
                }
#endif
                break;
            }
            default: { break; }
            }


            type = t;
            keybits = kb;
            return 0;
        }

        void libatgw_proto_inner_v1::crypt_session_t::close() {
            switch (type) {
            case ::atframe::gw::inner::v1::crypt_type_t_EN_ET_XXTEA: {
                // donothing
                break;
            }
            case ::atframe::gw::inner::v1::crypt_type_t_EN_ET_AES: {
#if defined(LIBATFRAME_ATGATEWAY_ENABLE_OPENSSL) || defined(LIBATFRAME_ATGATEWAY_ENABLE_LIBRESSL)
#elif defined(LIBATFRAME_ATGATEWAY_ENABLE_MBEDTLS)
                mbedtls_aes_free(&aes_key.mbedtls_aes_encrypt_ctx);
                mbedtls_aes_free(&aes_key.mbedtls_aes_decrypt_ctx);
#endif
                break;
            }
            default: { break; }
            }

            type = 0;
            keybits = 0;
        }

        libatgw_proto_inner_v1::libatgw_proto_inner_v1() : session_id_(0), last_write_ptr_(NULL), close_reason_(0) {
            crypt_handshake_ = std::make_shared<crypt_session_t>();

            read_head_.len = 0;

            ping_.last_ping = 0;
            ping_.last_delta = 0;

            handshake_.has_data = false;
        }

        libatgw_proto_inner_v1::~libatgw_proto_inner_v1() {
            close(close_reason_t::EN_CRT_UNKNOWN, false);
            close_handshake(error_code_t::EN_ECT_SESSION_EXPIRED);
        }

        void libatgw_proto_inner_v1::alloc_recv_buffer(size_t suggested_size, char *&out_buf, size_t &out_len) {
            flag_guard_t flag_guard(flags_, flag_t::EN_PFT_IN_CALLBACK);

            // 如果正处于关闭阶段，忽略所有数据
            if (check_flag(flag_t::EN_PFT_CLOSING)) {
                out_buf = NULL;
                out_len = 0;
                return;
            }

            void *data = NULL;
            size_t sread = 0, swrite = 0;
            read_buffers_.back(data, sread, swrite);

            // reading length and hash code, use small buffer block
            if (NULL == data || 0 == swrite) {
                out_len = sizeof(read_head_.buffer) - read_head_.len;

                if (0 == out_len) {
                    // hash code and length shouldn't be greater than small buffer block
                    out_buf = NULL;
                    assert(false);
                } else {
                    out_buf = &read_head_.buffer[read_head_.len];
                }
                return;
            }

            // 否则指定为大内存块缓冲区
            out_buf = reinterpret_cast<char *>(data);
            out_len = swrite;
        }

        void libatgw_proto_inner_v1::read(int ssz, const char *buff, size_t nread_s, int &errcode) {
            if (check_flag(flag_t::EN_PFT_CLOSING)) {
                errcode = error_code_t::EN_ECT_CLOSING;
                return;
            }

            errcode = error_code_t::EN_ECT_SUCCESS;
            flag_guard_t flag_guard(flags_, flag_t::EN_PFT_IN_CALLBACK);

            void *data = NULL;
            size_t sread = 0, swrite = 0;
            read_buffers_.back(data, sread, swrite);
            bool is_free = false;

            // first 32bits is hash code, and then 32bits length
            const size_t msg_header_len = sizeof(uint32_t) + sizeof(uint32_t);

            if (NULL == data || 0 == swrite) {
                // first, read from small buffer block
                // read header
                assert(nread_s <= sizeof(read_head_.buffer) - read_head_.len);
                read_head_.len += nread_s; // 写数据计数

                // try to unpack all messages
                char *buff_start = read_head_.buffer;
                size_t buff_left_len = read_head_.len;

                // maybe there are more than one message
                while (buff_left_len > sizeof(uint32_t) + sizeof(uint32_t)) {
                    uint32_t msg_len = 0;
                    msg_len = flatbuffers::ReadScalar<uint32_t>(buff_start + sizeof(uint32_t));

                    // directly dispatch small message
                    if (buff_left_len >= msg_header_len + msg_len) {
                        uint32_t check_hash = util::hash::murmur_hash3_x86_32(buff_start + msg_header_len, static_cast<int>(msg_len), 0);
                        uint32_t expect_hash;
                        memcpy(&expect_hash, buff_start, sizeof(uint32_t));

                        if (check_hash != expect_hash) {
                            errcode = error_code_t::EN_ECT_BAD_DATA;
                            // } else if (channel->conf.recv_buffer_limit_size > 0 && msg_len > channel->conf.recv_buffer_limit_size) {
                            //     errcode = EN_ATBUS_ERR_INVALID_SIZE;
                        }

                        // padding to 64bits
                        dispatch_data(reinterpret_cast<char *>(buff_start) + msg_header_len, msg_len, errcode);

                        // 32bits hash+vint+buffer
                        buff_start += msg_header_len + msg_len;
                        buff_left_len -= msg_header_len + msg_len;
                    } else {
                        // left data must be a big message
                        // store 32bits hash code + msg length
                        // keep padding
                        if (0 == read_buffers_.push_back(data, msg_header_len + msg_len)) {
                            memcpy(data, buff_start, buff_left_len);
                            read_buffers_.pop_back(buff_left_len, false);

                            buff_start += buff_left_len;
                            buff_left_len = 0; // exit the loop
                        } else {
                            // maybe message is too large
                            is_free = true;
                            buff_start += msg_header_len;
                            buff_left_len -= msg_header_len;
                            break;
                        }
                    }
                }

                // move left data to front
                if (buff_start != read_head_.buffer && buff_left_len > 0) {
                    memmove(read_head_.buffer, buff_start, buff_left_len);
                }
                read_head_.len = buff_left_len;
            } else {
                // mark data written
                read_buffers_.pop_back(nread_s, false);
            }

            // if big message recv done, dispatch it
            read_buffers_.front(data, sread, swrite);
            if (NULL != data && 0 == swrite) {
                data = reinterpret_cast<char *>(data) - sread;

                // 32bits hash code
                uint32_t check_hash = util::hash::murmur_hash3_x86_32(reinterpret_cast<char *>(data) + msg_header_len,
                                                                      static_cast<int>(sread - msg_header_len), 0);
                uint32_t expect_hash;
                memcpy(&expect_hash, data, sizeof(uint32_t));
                size_t msg_len = sread - msg_header_len;

                if (check_hash != expect_hash) {
                    errcode = error_code_t::EN_ECT_BAD_DATA;
                    // } else if (channel->conf.recv_buffer_limit_size > 0 && msg_len > channel->conf.recv_buffer_limit_size) {
                    //     errcode = EN_ATBUS_ERR_INVALID_SIZE;
                }

                dispatch_data(reinterpret_cast<char *>(data) + msg_header_len, msg_len, errcode);
                // free the buffer block
                read_buffers_.pop_front(0, true);
            }

            if (is_free) {
                errcode = error_code_t::EN_ECT_INVALID_SIZE;
                if (read_head_.len > 0) {
                    dispatch_data(read_head_.buffer, read_head_.len, errcode);
                }
            }
        }

        void libatgw_proto_inner_v1::dispatch_data(const char *buffer, size_t len, int errcode) {
            if (check_flag(flag_t::EN_PFT_CLOSING)) {
                return;
            }

            // do nothing if any error
            if (errcode < 0 || NULL == buffer) {
                return;
            }

            ::flatbuffers::Verifier cs_msg_verify(reinterpret_cast<const uint8_t *>(buffer), len);
            // verify
            if (false == atframe::gw::inner::v1::Verifycs_msgBuffer(cs_msg_verify)) {
                close(close_reason_t::EN_CRT_INVALID_DATA);
                return;
            }

            // unpack
            const atframe::gw::inner::v1::cs_msg *msg = atframe::gw::inner::v1::Getcs_msg(buffer);
            if (NULL == msg->head()) {
                return;
            }

            switch (msg->head()->type()) {
            case atframe::gw::inner::v1::cs_msg_type_t_EN_MTT_POST: {
                if (::atframe::gw::inner::v1::cs_msg_body_cs_body_post != msg->body_type()) {
                    close(close_reason_t::EN_CRT_INVALID_DATA, false);
                    break;
                }

                if (!check_flag(flag_t::EN_PFT_HANDSHAKE_DONE) || !crypt_read_) {
                    close(close_reason_t::EN_CRT_HANDSHAKE, false);
                    break;
                }

                const ::atframe::gw::inner::v1::cs_body_post *msg_body =
                    static_cast<const ::atframe::gw::inner::v1::cs_body_post *>(msg->body());

                const void *out;
                size_t outsz = static_cast<size_t>(msg_body->length());
                int res = decode_post(msg_body->data()->data(), static_cast<size_t>(msg_body->data()->size()), out, outsz);
                if (0 == res) {
                    // on_message
                    if (NULL != callbacks_ && callbacks_->message_fn) {
                        callbacks_->message_fn(this, out, static_cast<size_t>(msg_body->length()));
                    }
                } else {
                    close(close_reason_t::EN_CRT_INVALID_DATA, false);
                }

                break;
            }
            case atframe::gw::inner::v1::cs_msg_type_t_EN_MTT_HANDSHAKE:
            case atframe::gw::inner::v1::cs_msg_type_t_EN_MTT_POST_KEY_SYN:
            case atframe::gw::inner::v1::cs_msg_type_t_EN_MTT_POST_KEY_ACK: {
                if (::atframe::gw::inner::v1::cs_msg_body_cs_body_handshake != msg->body_type()) {
                    close(close_reason_t::EN_CRT_INVALID_DATA, false);
                    break;
                }
                const ::atframe::gw::inner::v1::cs_body_handshake *msg_body =
                    static_cast<const ::atframe::gw::inner::v1::cs_body_handshake *>(msg->body());
                dispatch_handshake(*msg_body);
                break;
            }
            case atframe::gw::inner::v1::cs_msg_type_t_EN_MTT_PING: {
                if (::atframe::gw::inner::v1::cs_msg_body_cs_body_ping != msg->body_type()) {
                    close(close_reason_t::EN_CRT_INVALID_DATA);
                    break;
                }

                const ::atframe::gw::inner::v1::cs_body_ping *msg_body =
                    static_cast<const ::atframe::gw::inner::v1::cs_body_ping *>(msg->body());

                // response pong
                ping_.last_ping = static_cast<time_t>(msg_body->timepoint());
                send_pong(ping_.last_ping);
                break;
            }
            case atframe::gw::inner::v1::cs_msg_type_t_EN_MTT_PONG: {
                if (::atframe::gw::inner::v1::cs_msg_body_cs_body_ping != msg->body_type()) {
                    close(close_reason_t::EN_CRT_INVALID_DATA);
                    break;
                }

                const ::atframe::gw::inner::v1::cs_body_ping *msg_body =
                    static_cast<const ::atframe::gw::inner::v1::cs_body_ping *>(msg->body());

                // update ping/pong duration
                if (0 != ping_.last_ping) {
                    ping_.last_delta = static_cast<time_t>(msg_body->timepoint()) - ping_.last_ping;
                }
                break;
            }
            case atframe::gw::inner::v1::cs_msg_type_t_EN_MTT_KICKOFF: {
                if (::atframe::gw::inner::v1::cs_msg_body_cs_body_kickoff != msg->body_type()) {
                    close(close_reason_t::EN_CRT_INVALID_DATA, false);
                    break;
                }

                const ::atframe::gw::inner::v1::cs_body_kickoff *msg_body =
                    static_cast<const ::atframe::gw::inner::v1::cs_body_kickoff *>(msg->body());
                close(msg_body->reason(), false);
                break;
            }
            default: { break; }
            }
        }

        int libatgw_proto_inner_v1::dispatch_handshake(const ::atframe::gw::inner::v1::cs_body_handshake &body_handshake) {
            if (check_flag(flag_t::EN_PFT_CLOSING)) {
                return error_code_t::EN_ECT_CLOSING;
            }

            if (check_flag(flag_t::EN_PFT_HANDSHAKE_DONE) && !check_flag(flag_t::EN_PFT_HANDSHAKE_UPDATE)) {
                return error_code_t::EN_ECT_HANDSHAKE;
            }

            int ret = 0;
            switch (body_handshake.step()) {
            case atframe::gw::inner::v1::handshake_step_t_EN_HST_START_REQ: {
                ret = dispatch_handshake_start_req(body_handshake);
                break;
            }
            case atframe::gw::inner::v1::handshake_step_t_EN_HST_START_RSP: {
                ret = dispatch_handshake_start_rsp(body_handshake);
                break;
            }
            case atframe::gw::inner::v1::handshake_step_t_EN_HST_RECONNECT_REQ: {
                ret = dispatch_handshake_reconn_req(body_handshake);
                break;
            }
            case atframe::gw::inner::v1::handshake_step_t_EN_HST_RECONNECT_RSP: {
                ret = dispatch_handshake_reconn_rsp(body_handshake);
                break;
            }
            case atframe::gw::inner::v1::handshake_step_t_EN_HST_DH_PUBKEY_REQ: {
                ret = dispatch_handshake_dh_pubkey_req(body_handshake);
                break;
            }
            case atframe::gw::inner::v1::handshake_step_t_EN_HST_DH_PUBKEY_RSP: {
                ret = dispatch_handshake_dh_pubkey_rsp(body_handshake);
                break;
            }
            case atframe::gw::inner::v1::handshake_step_t_EN_HST_RSA_SECRET_REQ: {
                ret = dispatch_handshake_rsa_secret_req(body_handshake);
                break;
            }
            case atframe::gw::inner::v1::handshake_step_t_EN_HST_RSA_SECRET_RSP: {
                ret = dispatch_handshake_rsa_secret_rsp(body_handshake);
                break;
            }
            case atframe::gw::inner::v1::handshake_step_t_EN_HST_VERIFY: {
                ret = dispatch_handshake_verify_ntf(body_handshake);
                break;
            }
            default: { break; }
            }

            // handshake failed will close the connection
            if (ret < 0) {
                close_handshake(ret);
                close(close_reason_t::EN_CRT_HANDSHAKE, false);
            }
            return ret;
        }

        int libatgw_proto_inner_v1::dispatch_handshake_start_req(const ::atframe::gw::inner::v1::cs_body_handshake &body_handshake) {
            if (check_flag(flag_t::EN_PFT_CLOSING)) {
                return error_code_t::EN_ECT_CLOSING;
            }

            if (NULL == callbacks_ || !callbacks_->write_fn || !callbacks_->new_session_fn) {
                return error_code_t::EN_ECT_MISS_CALLBACKS;
            }

            std::shared_ptr<detail::crypt_global_configure_t> global_cfg = detail::crypt_global_configure_t::current();
            int ret = setup_handshake(global_cfg);
            if (ret < 0) {
                return ret;
            }

            // check switch type
            if (body_handshake.switch_type() != handshake_.switch_secret_type) {
                ATFRAME_GATEWAY_ON_ERROR(error_code_t::EN_ECT_HANDSHAKE, "switch key type between client and server not matched.");
                close(error_code_t::EN_ECT_HANDSHAKE, true);
                return error_code_t::EN_ECT_HANDSHAKE;
            }

            callbacks_->new_session_fn(this, session_id_);

            flatbuffers::FlatBufferBuilder builder;
            ::atframe::gw::inner::v1::cs_msgBuilder msg(builder);
            msg.add_head(::atframe::gw::inner::v1::Createcs_msg_head(builder, ::atframe::gw::inner::v1::cs_msg_type_t_EN_MTT_HANDSHAKE,
                                                                     ::atframe::gateway::detail::alloc_seq()));
            ::atframe::gw::inner::v1::cs_body_handshakeBuilder handshake_body(builder);


            ret = pack_handshake_start_rsp(builder, session_id_, handshake_body);
            if (ret < 0) {
                return ret;
            }

            msg.add_body_type(::atframe::gw::inner::v1::cs_msg_body_cs_body_handshake);
            msg.add_body(handshake_body.Finish().Union());

            builder.Finish(msg.Finish());
            return write_msg(builder);
        }

        int libatgw_proto_inner_v1::dispatch_handshake_start_rsp(const ::atframe::gw::inner::v1::cs_body_handshake &body_handshake) {
            // check switch type
            // check if start new session success
            if (0 == body_handshake.session_id() || body_handshake.switch_type() != handshake_.switch_secret_type) {
                ATFRAME_GATEWAY_ON_ERROR(error_code_t::EN_ECT_HANDSHAKE, "start new session refused.");
                return error_code_t::EN_ECT_HANDSHAKE;
            }

            // assign session id,
            session_id_ = body_handshake.session_id();

            // if not use crypt, assign crypt information and close_handshake(0)
            if (::atframe::gw::inner::v1::crypt_type_t_EN_ET_NONE == body_handshake.crypt_type()) {
                crypt_handshake_->setup(::atframe::gw::inner::v1::crypt_type_t_EN_ET_NONE, 0);
                crypt_read_ = crypt_handshake_;
                crypt_write_ = crypt_handshake_;
                close_handshake(0);
                return 0;
            }

            if (NULL == body_handshake.crypt_param()) {
                ATFRAME_GATEWAY_ON_ERROR(error_code_t::EN_ECT_HANDSHAKE, "has no secret");
                return error_code_t::EN_ECT_HANDSHAKE;
            }

            int ret = 0;
            switch (handshake_.switch_secret_type) {
            case ::atframe::gw::inner::v1::switch_secret_t_EN_SST_DIRECT: {
                crypt_handshake_->secret.assign(reinterpret_cast<const char *>(body_handshake.crypt_param()->data()),
                                                body_handshake.crypt_param()->size());
                crypt_handshake_->setup(body_handshake.crypt_type(), body_handshake.crypt_bits());
                crypt_read_ = crypt_handshake_;
                crypt_write_ = crypt_handshake_;
                close_handshake(0);

                // send verify
                ret = send_verify(NULL, 0);
                break;
            }
            case ::atframe::gw::inner::v1::switch_secret_t_EN_SST_DH: {
                // if in DH handshake, generate and send pubkey
                flatbuffers::FlatBufferBuilder builder;
                ::atframe::gw::inner::v1::cs_msgBuilder msg(builder);
                msg.add_head(::atframe::gw::inner::v1::Createcs_msg_head(builder, ::atframe::gw::inner::v1::cs_msg_type_t_EN_MTT_HANDSHAKE,
                                                                         ::atframe::gateway::detail::alloc_seq()));
                ::atframe::gw::inner::v1::cs_body_handshakeBuilder dh_pubkey_body(builder);


                ret = pack_handshake_dh_pubkey_req(builder, body_handshake, dh_pubkey_body);
                if (ret < 0) {
                    break;
                }

                msg.add_body_type(::atframe::gw::inner::v1::cs_msg_body_cs_body_handshake);
                msg.add_body(dh_pubkey_body.Finish().Union());

                builder.Finish(msg.Finish());
                ret = write_msg(builder);
                break;
            }
            case ::atframe::gw::inner::v1::switch_secret_t_EN_SST_RSA: {
                // TODO RSA not support now
                // TODO if in RSA handshake, generate and send secret
                ATFRAME_GATEWAY_ON_ERROR(error_code_t::EN_ECT_HANDSHAKE, "not support now");
                ret = error_code_t::EN_ECT_HANDSHAKE;
                break;
            }
            default: {
                ATFRAME_GATEWAY_ON_ERROR(handshake_.switch_secret_type, "unsupported switch type");
                ret = error_code_t::EN_ECT_HANDSHAKE;
                break;
            }
            }

            return ret;
        }

        int libatgw_proto_inner_v1::dispatch_handshake_reconn_req(const ::atframe::gw::inner::v1::cs_body_handshake &body_handshake) {
            // try to reconnect
            if (NULL == callbacks_ || !callbacks_->reconnect_fn) {
                return error_code_t::EN_ECT_MISS_CALLBACKS;
            }

            // assign crypt options
            const flatbuffers::Vector<int8_t> *secret = body_handshake.crypt_param();
            if (NULL != secret) {
                crypt_handshake_->secret.assign(reinterpret_cast<const char *>(secret->data()), secret->size());
            }

            int ret = callbacks_->reconnect_fn(this, body_handshake.session_id());
            if (ret >= 0 &&
                (crypt_handshake_->type != body_handshake.crypt_type() || crypt_handshake_->keybits != body_handshake.crypt_bits())) {
                ATFRAME_GATEWAY_ON_ERROR(error_code_t::EN_ECT_HANDSHAKE, "reconnect success but crypt type not matched");
                ret = error_code_t::EN_ECT_HANDSHAKE;
            }

            flatbuffers::FlatBufferBuilder builder;
            ::atframe::gw::inner::v1::cs_msgBuilder msg(builder);
            msg.add_head(::atframe::gw::inner::v1::Createcs_msg_head(builder, ::atframe::gw::inner::v1::cs_msg_type_t_EN_MTT_HANDSHAKE,
                                                                     ::atframe::gateway::detail::alloc_seq()));
            ::atframe::gw::inner::v1::cs_body_handshakeBuilder reconn_body(builder);


            if (0 == ret) {
                reconn_body.add_session_id(session_id_);
            } else {
                reconn_body.add_session_id(0);
            }
            reconn_body.add_step(::atframe::gw::inner::v1::handshake_step_t_EN_HST_RECONNECT_RSP);

            // copy data
            reconn_body.add_switch_type(static_cast< ::atframe::gw::inner::v1::switch_secret_t>(handshake_.switch_secret_type));
            reconn_body.add_crypt_type(static_cast< ::atframe::gw::inner::v1::crypt_type_t>(crypt_handshake_->type));
            reconn_body.add_crypt_bits(crypt_handshake_->keybits);

            msg.add_body_type(::atframe::gw::inner::v1::cs_msg_body_cs_body_handshake);
            msg.add_body(reconn_body.Finish().Union());

            builder.Finish(msg.Finish());

            if (0 != ret) {
                close_handshake(ret);
                close(ret, true);
                write_msg(builder);
            } else {
                ret = write_msg(builder);
                close_handshake(ret);
            }

            return ret;
        }

        int libatgw_proto_inner_v1::dispatch_handshake_reconn_rsp(const ::atframe::gw::inner::v1::cs_body_handshake &body_handshake) {
            // if success, session id is not 0, and assign all data
            if (0 == body_handshake.session_id()) {
                ATFRAME_GATEWAY_ON_ERROR(error_code_t::EN_ECT_HANDSHAKE, "start new session refused.");
                return error_code_t::EN_ECT_HANDSHAKE;
            }

            session_id_ = body_handshake.session_id();
            close_handshake(0);
            return 0;
        }

        int libatgw_proto_inner_v1::dispatch_handshake_dh_pubkey_req(const ::atframe::gw::inner::v1::cs_body_handshake &peer_body) {
            // check
            int ret = 0;
            if (handshake_.switch_secret_type != peer_body.switch_type() || !crypt_handshake_->shared_conf ||
                crypt_handshake_->shared_conf->conf_.type != peer_body.crypt_type() ||
                crypt_handshake_->shared_conf->conf_.keybits != peer_body.crypt_bits()) {
                ATFRAME_GATEWAY_ON_ERROR(error_code_t::EN_ECT_HANDSHAKE, "crypt information between client and server not matched.");
                close(error_code_t::EN_ECT_HANDSHAKE, true);
                return error_code_t::EN_ECT_HANDSHAKE;
            }

            flatbuffers::FlatBufferBuilder builder;
            ::atframe::gw::inner::v1::cs_msgBuilder msg(builder);
            msg.add_head(::atframe::gw::inner::v1::Createcs_msg_head(builder, ::atframe::gw::inner::v1::cs_msg_type_t_EN_MTT_HANDSHAKE,
                                                                     ::atframe::gateway::detail::alloc_seq()));
            ::atframe::gw::inner::v1::cs_body_handshakeBuilder pubkey_rsp_body(builder);

            pubkey_rsp_body.add_session_id(session_id_);
            pubkey_rsp_body.add_step(::atframe::gw::inner::v1::handshake_step_t_EN_HST_DH_PUBKEY_RSP);

            // copy data
            pubkey_rsp_body.add_switch_type(peer_body.switch_type());
            pubkey_rsp_body.add_crypt_type(peer_body.crypt_type());
            pubkey_rsp_body.add_crypt_bits(peer_body.crypt_bits());
            crypt_handshake_->param.clear();

            do {
#if defined(LIBATFRAME_ATGATEWAY_ENABLE_OPENSSL) || defined(LIBATFRAME_ATGATEWAY_ENABLE_LIBRESSL)
                if (NULL == handshake_.dh.openssl_dh_ptr_ || false == handshake_.has_data) {
                    ret = error_code_t::EN_ECT_CRYPT_NOT_SUPPORTED;
                    ATFRAME_GATEWAY_ON_ERROR(ret, "openssl/libressl DH not setup");
                    break;
                }

                // ===============================================
                // generate next_secret
                BIGNUM *pubkey = BN_bin2bn(reinterpret_cast<const unsigned char *>(peer_body.crypt_param()->data()),
                                           peer_body.crypt_param()->size(), NULL);
                if (NULL == pubkey) {
                    ret = error_code_t::EN_ECT_CRYPT_NOT_SUPPORTED;
                    ATFRAME_GATEWAY_ON_ERROR(ret, "openssl/libressl DH decode public key failed");
                    break;
                };
                crypt_handshake_->secret.resize(static_cast<size_t>(sizeof(unsigned char) * (DH_size(handshake_.dh.openssl_dh_ptr_))), 0);
                int secret_len =
                    DH_compute_key(reinterpret_cast<unsigned char *>(&crypt_handshake_->secret[0]), pubkey, handshake_.dh.openssl_dh_ptr_);
                BN_free(pubkey);
                if (secret_len < 0) {
                    ret = error_code_t::EN_ECT_CRYPT_NOT_SUPPORTED;
                    ATFRAME_GATEWAY_ON_ERROR(secret_len, "openssl/libressl DH compute key failed");
                    break;
                }

                crypt_handshake_->setup(peer_body.crypt_type(), peer_body.crypt_bits());
                crypt_read_ = crypt_handshake_;
                crypt_write_ = crypt_handshake_;
#elif defined(LIBATFRAME_ATGATEWAY_ENABLE_MBEDTLS)
                if (false == handshake_.has_data) {
                    ret = error_code_t::EN_ECT_CRYPT_NOT_SUPPORTED;
                    ATFRAME_GATEWAY_ON_ERROR(ret, "mbedtls DH not setup");
                    break;
                }

                int res = mbedtls_dhm_read_public(&handshake_.dh.mbedtls_dh_ctx_,
                                                  reinterpret_cast<const unsigned char *>(peer_body.crypt_param()->data()),
                                                  peer_body.crypt_param()->size());

                if (0 != res) {
                    ATFRAME_GATEWAY_ON_ERROR(res, "mbedtls DH read param failed");
                    ret = error_code_t::EN_ECT_CRYPT_NOT_SUPPORTED;
                    break;
                }

                size_t psz = handshake_.dh.mbedtls_dh_ctx_.len;
                // generate next_secret
                crypt_handshake_->secret.resize(psz, 0);
                res =
                    mbedtls_dhm_calc_secret(&handshake_.dh.mbedtls_dh_ctx_, reinterpret_cast<unsigned char *>(&crypt_handshake_->secret[0]),
                                            psz, &psz, mbedtls_ctr_drbg_random, &handshake_.dh.mbedtls_ctr_drbg_);
                if (0 != res) {
                    ATFRAME_GATEWAY_ON_ERROR(res, "mbedtls DH compute key failed");
                    ret = error_code_t::EN_ECT_CRYPT_NOT_SUPPORTED;
                    break;
                }

                crypt_handshake_->setup(peer_body.crypt_type(), peer_body.crypt_bits());
                crypt_read_ = crypt_handshake_;
                crypt_write_ = crypt_handshake_;
#endif
            } while (false);

            // send verify text prefix
            const void *outbuf = NULL;
            size_t outsz = 0;
            if (0 == ret) {
                ret = encrypt_data(*crypt_handshake_, crypt_handshake_->shared_conf->conf_.default_key.data(),
                                   crypt_handshake_->shared_conf->conf_.default_key.size(), outbuf, outsz);
            }
            if (0 == ret) {
                crypt_handshake_->param.assign(reinterpret_cast<const char *>(outbuf), outsz);
                pubkey_rsp_body.add_crypt_param(
                    builder.CreateVector(reinterpret_cast<const int8_t *>(crypt_handshake_->param.data()), crypt_handshake_->param.size()));
            } else {
                pubkey_rsp_body.add_session_id(0);
                pubkey_rsp_body.add_crypt_param(builder.CreateVector(reinterpret_cast<const int8_t *>(NULL), 0));
            }

            msg.add_body_type(::atframe::gw::inner::v1::cs_msg_body_cs_body_handshake);
            msg.add_body(pubkey_rsp_body.Finish().Union());

            builder.Finish(msg.Finish());
            ret = write_msg(builder);
            return ret;
        }

        int libatgw_proto_inner_v1::dispatch_handshake_dh_pubkey_rsp(const ::atframe::gw::inner::v1::cs_body_handshake &body_handshake) {
            if (0 == body_handshake.session_id() || NULL == body_handshake.crypt_param()) {
                ATFRAME_GATEWAY_ON_ERROR(error_code_t::EN_ECT_HANDSHAKE, "DH switch key failed.");
                return error_code_t::EN_ECT_HANDSHAKE;
            }

            const void *outbuf = NULL;
            size_t outsz = 0;
            // decrypt default key
            int ret =
                decrypt_data(*crypt_handshake_, body_handshake.crypt_param()->data(), body_handshake.crypt_param()->size(), outbuf, outsz);
            if (0 == ret) {
                // secret already setuped when pack pubkey req
                crypt_read_ = crypt_handshake_;

                // add something and encrypt it again. and send verify message
                std::string verify_data;
                if (crypt_handshake_->shared_conf) {
                    verify_data.reserve(outsz + crypt_handshake_->shared_conf->conf_.default_key.size());
                    verify_data.assign(reinterpret_cast<const char *>(outbuf), outsz);
                    verify_data.append(crypt_handshake_->shared_conf->conf_.default_key.c_str(),
                                       crypt_handshake_->shared_conf->conf_.default_key.size());

                    ret = send_verify(verify_data.data(), verify_data.size());
                } else {
                    ret = send_verify(outbuf, outsz);
                }

                close_handshake(ret);
            }

            return ret;
        }

        int libatgw_proto_inner_v1::dispatch_handshake_rsa_secret_req(const ::atframe::gw::inner::v1::cs_body_handshake &body_handshake) {
            return 0;
        }

        int libatgw_proto_inner_v1::dispatch_handshake_rsa_secret_rsp(const ::atframe::gw::inner::v1::cs_body_handshake &body_handshake) {
            return 0;
        }

        int libatgw_proto_inner_v1::dispatch_handshake_verify_ntf(const ::atframe::gw::inner::v1::cs_body_handshake &body_handshake) {
            // check crypt info
            int ret = 0;
            if (handshake_.switch_secret_type != body_handshake.switch_type() || !crypt_handshake_->shared_conf ||
                crypt_handshake_->shared_conf->conf_.type != body_handshake.crypt_type() ||
                crypt_handshake_->shared_conf->conf_.keybits != body_handshake.crypt_bits()) {
                ATFRAME_GATEWAY_ON_ERROR(error_code_t::EN_ECT_HANDSHAKE, "crypt information between client and server not matched.");
                close(error_code_t::EN_ECT_HANDSHAKE, true);
                return error_code_t::EN_ECT_HANDSHAKE;
            }

            // check hello message prefix
            if (NULL == body_handshake.crypt_param() || crypt_handshake_->param.size() > body_handshake.crypt_param()->size()) {
                const char *checked_ch = reinterpret_cast<const char *>(body_handshake.crypt_param()->data());
                for (size_t i = 0; checked_ch && *checked_ch && i < crypt_handshake_->param.size(); ++i, ++checked_ch) {
                    if (*checked_ch != crypt_handshake_->param[i]) {
                        ret = error_code_t::EN_ECT_HANDSHAKE;
                        break;
                    }
                }
            }

            if (0 == ret) {
                // than read key updated
                close_handshake(0);
            } else {
                ATFRAME_GATEWAY_ON_ERROR(error_code_t::EN_ECT_HANDSHAKE, "verify failed.");
                close(error_code_t::EN_ECT_HANDSHAKE, true);
            }
            return ret;
        }

        int libatgw_proto_inner_v1::pack_handshake_start_rsp(flatbuffers::FlatBufferBuilder &builder, uint64_t sess_id,
                                                             ::atframe::gw::inner::v1::cs_body_handshakeBuilder &handshake_body) {

            handshake_body.add_session_id(sess_id);
            handshake_body.add_step(::atframe::gw::inner::v1::handshake_step_t_EN_HST_START_RSP);

            int ret = 0;
            // if not use crypt, assign crypt information and close_handshake(0)
            if (0 == sess_id || !crypt_handshake_->shared_conf ||
                ::atframe::gw::inner::v1::crypt_type_t_EN_ET_NONE == crypt_handshake_->shared_conf->conf_.type) {
                // empty data
                handshake_body.add_switch_type(::atframe::gw::inner::v1::switch_secret_t_EN_SST_DIRECT);
                handshake_body.add_crypt_type(::atframe::gw::inner::v1::crypt_type_t_EN_ET_NONE);
                handshake_body.add_crypt_bits(0);
                handshake_body.add_crypt_param(builder.CreateVector(reinterpret_cast<const int8_t *>(NULL), 0));

                crypt_handshake_->setup(::atframe::gw::inner::v1::crypt_type_t_EN_ET_NONE, 0);
                crypt_read_ = crypt_handshake_;
                crypt_write_ = crypt_handshake_;
                close_handshake(0);
                return ret;
            }

            // use the global switch type
            handshake_body.add_switch_type(static_cast< ::atframe::gw::inner::v1::switch_secret_t>(handshake_.switch_secret_type));
            // use the global crypt type
            handshake_body.add_crypt_type(static_cast< ::atframe::gw::inner::v1::crypt_type_t>(crypt_handshake_->shared_conf->conf_.type));
            handshake_body.add_crypt_bits(crypt_handshake_->shared_conf->conf_.keybits);
            crypt_handshake_->param.clear();

            switch (handshake_.switch_secret_type) {
            case ::atframe::gw::inner::v1::switch_secret_t_EN_SST_DIRECT: {
                // TODO generate a secret key
                crypt_handshake_->secret = crypt_handshake_->shared_conf->conf_.default_key;
                crypt_handshake_->setup(crypt_handshake_->shared_conf->conf_.type, crypt_handshake_->shared_conf->conf_.keybits);

                crypt_write_ = crypt_handshake_;
                handshake_body.add_crypt_param(builder.CreateVector(reinterpret_cast<const int8_t *>(crypt_handshake_->secret.data()),
                                                                    crypt_handshake_->secret.size()));
                break;
            }
            case ::atframe::gw::inner::v1::switch_secret_t_EN_SST_DH: {

                do {
#if defined(LIBATFRAME_ATGATEWAY_ENABLE_OPENSSL) || defined(LIBATFRAME_ATGATEWAY_ENABLE_LIBRESSL)
                    if (NULL == handshake_.dh.openssl_dh_ptr_ || false == handshake_.has_data) {
                        ret = error_code_t::EN_ECT_CRYPT_NOT_SUPPORTED;
                        ATFRAME_GATEWAY_ON_ERROR(ret, "openssl/libressl DH not setup");
                        break;
                    }

                    // ===============================================
                    int res = DH_generate_key(handshake_.dh.openssl_dh_ptr_);
                    if (1 != res) {
                        ret = error_code_t::EN_ECT_CRYPT_NOT_SUPPORTED;
                        ATFRAME_GATEWAY_ON_ERROR(ret, "openssl/libressl DH generate public key failed");
                        break;
                    }
                    int errcode = 0;
                    res = DH_check_pub_key(handshake_.dh.openssl_dh_ptr_, handshake_.dh.openssl_dh_ptr_->pub_key, &errcode);
                    if (res != 1) {
                        ATFRAME_GATEWAY_ON_ERROR(errcode, "openssl/libressl DH generate check public key failed");
                        ret = error_code_t::EN_ECT_CRYPT_NOT_SUPPORTED;
                        break;
                    }

                    // write big number into buffer, the size must be no less than BN_num_bytes()
                    // @see https://www.openssl.org/docs/manmaster/crypto/BN_bn2bin.html
                    size_t dhparam_bnsz = BN_num_bytes(handshake_.dh.openssl_dh_ptr_->pub_key);
                    crypt_handshake_->param.resize(dhparam_bnsz, 0);
                    BN_bn2bin(handshake_.dh.openssl_dh_ptr_->pub_key, reinterpret_cast<unsigned char *>(&crypt_handshake_->param[0]));

#elif defined(LIBATFRAME_ATGATEWAY_ENABLE_MBEDTLS)
                    if (false == handshake_.has_data) {
                        ret = error_code_t::EN_ECT_CRYPT_NOT_SUPPORTED;
                        ATFRAME_GATEWAY_ON_ERROR(ret, "mbedtls DH not setup");
                        break;
                    }

                    size_t psz = mbedtls_mpi_size(&handshake_.dh.mbedtls_dh_ctx_.P);
                    size_t olen = 0;
                    crypt_handshake_->param.resize(psz, 0);
                    int res = mbedtls_dhm_make_params(&handshake_.dh.mbedtls_dh_ctx_, static_cast<int>(psz),
                                                      reinterpret_cast<unsigned char *>(&crypt_handshake_->param[0]), &olen,
                                                      mbedtls_ctr_drbg_random, &handshake_.dh.mbedtls_ctr_drbg_);
                    if (0 != res) {
                        ATFRAME_GATEWAY_ON_ERROR(res, "mbedtls DH generate check public key failed");
                        ret = error_code_t::EN_ECT_CRYPT_NOT_SUPPORTED;
                        break;
                    }

                    assert(olen <= psz);
                    if (olen < psz) {
                        crypt_handshake_->param.resize(olen);
                    }
#endif

                } while (false);
                // send send first parameter
                handshake_body.add_crypt_param(
                    builder.CreateVector(reinterpret_cast<const int8_t *>(crypt_handshake_->param.data()), crypt_handshake_->param.size()));
                break;
            }
            case ::atframe::gw::inner::v1::switch_secret_t_EN_SST_RSA: {
                ret = error_code_t::EN_ECT_CRYPT_NOT_SUPPORTED;
                ATFRAME_GATEWAY_ON_ERROR(ret, "RSA not supported now");
                break;
            }
            default: {
                ret = error_code_t::EN_ECT_CRYPT_NOT_SUPPORTED;
                ATFRAME_GATEWAY_ON_ERROR(ret, "RSA not supported now");
                break;
            }
            }

            // TODO if switch type is RSA
            // ::atframe::gw::inner::v1::cs_body_rsa_certBuilder rsa_cert(builder);
            // rsa_cert.add_rsa_sign(::atframe::gw::inner::v1::rsa_sign_t_EN_RST_PKCS1);
            // rsa_cert.add_hash_type(::atframe::gw::inner::v1::hash_id_t_EN_HIT_MD5);
            // rsa_cert.add_pubkey(builder.CreateVector(NULL, 0));
            // handshake_body.add_rsa_cert(rsa_cert.Finish());

            return ret;
        }

        int libatgw_proto_inner_v1::pack_handshake_dh_pubkey_req(flatbuffers::FlatBufferBuilder &builder,
                                                                 const ::atframe::gw::inner::v1::cs_body_handshake &peer_body,
                                                                 ::atframe::gw::inner::v1::cs_body_handshakeBuilder &handshake_body) {

            handshake_body.add_session_id(peer_body.session_id());
            handshake_body.add_step(::atframe::gw::inner::v1::handshake_step_t_EN_HST_DH_PUBKEY_REQ);

            int ret = 0;
            if (0 == peer_body.session_id() || NULL == peer_body.crypt_param()) {
                // empty data
                return error_code_t::EN_ECT_SESSION_NOT_FOUND;
            }

            handshake_.switch_secret_type = peer_body.switch_type();
            // use the global switch type
            handshake_body.add_switch_type(static_cast< ::atframe::gw::inner::v1::switch_secret_t>(handshake_.switch_secret_type));
            // use the global crypt type

            handshake_body.add_crypt_type(peer_body.crypt_type());
            handshake_body.add_crypt_bits(peer_body.crypt_bits());
            crypt_handshake_->param.clear();

            do {
#if defined(LIBATFRAME_ATGATEWAY_ENABLE_OPENSSL) || defined(LIBATFRAME_ATGATEWAY_ENABLE_LIBRESSL)
                if (NULL == handshake_.dh.openssl_dh_ptr_ || false == handshake_.has_data) {
                    ret = error_code_t::EN_ECT_CRYPT_NOT_SUPPORTED;
                    ATFRAME_GATEWAY_ON_ERROR(ret, "openssl/libressl DH not setup");
                    break;
                }

                // ===============================================
                int res = DH_generate_key(handshake_.dh.openssl_dh_ptr_);
                if (1 != res) {
                    ret = error_code_t::EN_ECT_CRYPT_NOT_SUPPORTED;
                    ATFRAME_GATEWAY_ON_ERROR(ret, "openssl/libressl DH generate public key failed");
                    break;
                }
                int errcode = 0;
                res = DH_check_pub_key(handshake_.dh.openssl_dh_ptr_, handshake_.dh.openssl_dh_ptr_->pub_key, &errcode);
                if (res != 1) {
                    ATFRAME_GATEWAY_ON_ERROR(errcode, "openssl/libressl DH generate check public key failed");
                    ret = error_code_t::EN_ECT_CRYPT_NOT_SUPPORTED;
                    break;
                }

                // write big number into buffer, the size must be no less than BN_num_bytes()
                // @see https://www.openssl.org/docs/manmaster/crypto/BN_bn2bin.html
                size_t dhparam_bnsz = BN_num_bytes(handshake_.dh.openssl_dh_ptr_->pub_key);
                crypt_handshake_->param.resize(dhparam_bnsz, 0);
                BN_bn2bin(handshake_.dh.openssl_dh_ptr_->pub_key, reinterpret_cast<unsigned char *>(&crypt_handshake_->param[0]));

                // generate next_secret
                BIGNUM *pubkey = BN_bin2bn(reinterpret_cast<const unsigned char *>(peer_body.crypt_param()->data()),
                                           peer_body.crypt_param()->size(), NULL);
                if (NULL == pubkey) {
                    ret = error_code_t::EN_ECT_CRYPT_NOT_SUPPORTED;
                    ATFRAME_GATEWAY_ON_ERROR(ret, "openssl/libressl DH decode public key failed");
                    break;
                };
                crypt_handshake_->secret.resize(static_cast<size_t>(sizeof(unsigned char) * (DH_size(handshake_.dh.openssl_dh_ptr_))), 0);
                errcode =
                    DH_compute_key(reinterpret_cast<unsigned char *>(&crypt_handshake_->secret[0]), pubkey, handshake_.dh.openssl_dh_ptr_);
                BN_free(pubkey);
                if (errcode < 0) {
                    ret = error_code_t::EN_ECT_CRYPT_NOT_SUPPORTED;
                    ATFRAME_GATEWAY_ON_ERROR(errcode, "openssl/libressl DH compute key failed");
                    break;
                }
                crypt_handshake_->setup(peer_body.crypt_type(), peer_body.crypt_bits());
                crypt_write_ = crypt_handshake_;

#elif defined(LIBATFRAME_ATGATEWAY_ENABLE_MBEDTLS)
                if (false == handshake_.has_data) {
                    ret = error_code_t::EN_ECT_CRYPT_NOT_SUPPORTED;
                    ATFRAME_GATEWAY_ON_ERROR(ret, "mbedtls DH not setup");
                    break;
                }

                unsigned char *dh_params_beg =
                    const_cast<unsigned char *>(reinterpret_cast<const unsigned char *>(peer_body.crypt_param()->data()));
                int res = mbedtls_dhm_read_params(&handshake_.dh.mbedtls_dh_ctx_, &dh_params_beg,
                                                  dh_params_beg + peer_body.crypt_param()->size());
                if (0 != res) {
                    ATFRAME_GATEWAY_ON_ERROR(res, "mbedtls DH read param failed");
                    ret = error_code_t::EN_ECT_CRYPT_NOT_SUPPORTED;
                    break;
                }

                size_t psz = handshake_.dh.mbedtls_dh_ctx_.len;
                crypt_handshake_->param.resize(psz, 0);
                res = mbedtls_dhm_make_public(&handshake_.dh.mbedtls_dh_ctx_, static_cast<int>(psz),
                                              reinterpret_cast<unsigned char *>(&crypt_handshake_->param[0]), psz, mbedtls_ctr_drbg_random,
                                              &handshake_.dh.mbedtls_ctr_drbg_);
                if (0 != res) {
                    ATFRAME_GATEWAY_ON_ERROR(res, "mbedtls DH make public key failed");
                    ret = error_code_t::EN_ECT_CRYPT_NOT_SUPPORTED;
                    break;
                }

                // generate secret
                crypt_handshake_->secret.resize(psz, 0);
                res =
                    mbedtls_dhm_calc_secret(&handshake_.dh.mbedtls_dh_ctx_, reinterpret_cast<unsigned char *>(&crypt_handshake_->secret[0]),
                                            psz, &psz, mbedtls_ctr_drbg_random, &handshake_.dh.mbedtls_ctr_drbg_);
                if (0 != res) {
                    ATFRAME_GATEWAY_ON_ERROR(res, "mbedtls DH compute key failed");
                    ret = error_code_t::EN_ECT_CRYPT_NOT_SUPPORTED;
                    break;
                }

                crypt_handshake_->setup(peer_body.crypt_type(), peer_body.crypt_bits());
                crypt_write_ = crypt_handshake_;
#endif
            } while (false);

            // send send first parameter
            handshake_body.add_crypt_param(
                builder.CreateVector(reinterpret_cast<const int8_t *>(crypt_handshake_->param.data()), crypt_handshake_->param.size()));

            // TODO if switch type is RSA
            // ::atframe::gw::inner::v1::cs_body_rsa_certBuilder rsa_cert(builder);
            // rsa_cert.add_rsa_sign(::atframe::gw::inner::v1::rsa_sign_t_EN_RST_PKCS1);
            // rsa_cert.add_hash_type(::atframe::gw::inner::v1::hash_id_t_EN_HIT_MD5);
            // rsa_cert.add_pubkey(builder.CreateVector(NULL, 0));
            // handshake_body.add_rsa_cert(rsa_cert.Finish());

            return ret;
        }

        int libatgw_proto_inner_v1::setup_handshake(std::shared_ptr<detail::crypt_global_configure_t> &shared_conf) {
            if (handshake_.has_data) {
                return 0;
            }

            if (crypt_handshake_->shared_conf != shared_conf) {
                crypt_handshake_->shared_conf = shared_conf;
            }

            int ret = 0;
            if (!crypt_handshake_->shared_conf ||
                ::atframe::gw::inner::v1::crypt_type_t_EN_ET_NONE == crypt_handshake_->shared_conf->conf_.type) {
                return ret;
            }

            handshake_.switch_secret_type = crypt_handshake_->shared_conf->conf_.switch_secret_type;
            switch (handshake_.switch_secret_type) {
            case ::atframe::gw::inner::v1::switch_secret_t_EN_SST_DH: {
// init DH param file
#if defined(LIBATFRAME_ATGATEWAY_ENABLE_OPENSSL) || defined(LIBATFRAME_ATGATEWAY_ENABLE_LIBRESSL)
                handshake_.dh.openssl_dh_ptr_ = NULL;
                do {
                    handshake_.dh.openssl_dh_ptr_ = PEM_read_bio_DHparams(crypt_handshake_->shared_conf->openssl_dh_bio_, NULL, NULL, NULL);
                    if (!handshake_.dh.openssl_dh_ptr_) {
                        ret = error_code_t::EN_ECT_CRYPT_INIT_DHPARAM;
                        ATFRAME_GATEWAY_ON_ERROR(ret, "openssl/libressl parse dhm failed");
                        break;
                    }

                    if (1 != DH_generate_key(handshake_.dh.openssl_dh_ptr_)) {
                        ret = error_code_t::EN_ECT_CRYPT_INIT_DHPARAM;
                        ATFRAME_GATEWAY_ON_ERROR(ret, "openssl/libressl generate key failed");
                        break;
                    }
                } while (false);

                if (0 != ret) {
                    if (NULL != handshake_.dh.openssl_dh_ptr_) {
                        DH_free(handshake_.dh.openssl_dh_ptr_);
                        handshake_.dh.openssl_dh_ptr_ = NULL;
                    }
                }
// PEM_read_DHparams
#elif defined(LIBATFRAME_ATGATEWAY_ENABLE_MBEDTLS)
                // mbedtls_dhm_read_params
                do {
                    mbedtls_dhm_init(&handshake_.dh.mbedtls_dh_ctx_);
                    mbedtls_ctr_drbg_init(&handshake_.dh.mbedtls_ctr_drbg_);
                    mbedtls_entropy_init(&handshake_.dh.mbedtls_entropy_);

                    int res = mbedtls_ctr_drbg_seed(&handshake_.dh.mbedtls_ctr_drbg_, mbedtls_entropy_func, &handshake_.dh.mbedtls_entropy_,
                                                    NULL, 0);
                    if (0 != res) {
                        ret = error_code_t::EN_ECT_CRYPT_INIT_DHPARAM;
                        ATFRAME_GATEWAY_ON_ERROR(res, "mbedtls setup seed failed");
                        break;
                    }

                    res = mbedtls_dhm_parse_dhm(
                        &handshake_.dh.mbedtls_dh_ctx_,
                        reinterpret_cast<const unsigned char *>(crypt_handshake_->shared_conf->mbedtls_dh_param_.data()),
                        crypt_handshake_->shared_conf->mbedtls_dh_param_.size());
                    if (0 != res) {
                        ATFRAME_GATEWAY_ON_ERROR(res, "mbedtls parse dhm failed");
                        ret = error_code_t::EN_ECT_CRYPT_INIT_DHPARAM;
                        break;
                    }
                } while (false);

                if (0 != ret) {
                    mbedtls_ctr_drbg_free(&handshake_.dh.mbedtls_ctr_drbg_);
                    mbedtls_entropy_free(&handshake_.dh.mbedtls_entropy_);
                    mbedtls_dhm_free(&handshake_.dh.mbedtls_dh_ctx_);
                }
#endif
                break;
            }
            case ::atframe::gw::inner::v1::switch_secret_t_EN_SST_RSA: {
                // TODO init public key and private key
                // mbedtls: use API in mbedtls/pk.h => mbedtls_pk_parse_public_key, mbedtls_pk_parse_keyfile
                // openssl/libressl: use API in openssl/rsa.h,openssl/pem.h => RSA * = PEM_read_RSA_PUBKEY, PEM_read_RSAPrivateKey
                ret = error_code_t::EN_ECT_CRYPT_NOT_SUPPORTED;
                break;
            }
            case ::atframe::gw::inner::v1::switch_secret_t_EN_SST_DIRECT: {
                break;
            }
            default: {
                ret = error_code_t::EN_ECT_CRYPT_NOT_SUPPORTED;
                break;
            }
            }

            handshake_.has_data = 0 == ret;
            return ret;
        }

        void libatgw_proto_inner_v1::close_handshake(int status) {
            crypt_handshake_->param.clear();

            if (!handshake_.has_data) {
                handshake_done(status);
                return;
            }
            handshake_.has_data = false;

            switch (handshake_.switch_secret_type) {
            case ::atframe::gw::inner::v1::switch_secret_t_EN_SST_DH: {
// free DH param file
#if defined(LIBATFRAME_ATGATEWAY_ENABLE_OPENSSL) || defined(LIBATFRAME_ATGATEWAY_ENABLE_LIBRESSL)
                if (NULL != handshake_.dh.openssl_dh_ptr_) {
                    DH_free(handshake_.dh.openssl_dh_ptr_);
                    handshake_.dh.openssl_dh_ptr_ = NULL;
                }
// PEM_read_DHparams
#elif defined(LIBATFRAME_ATGATEWAY_ENABLE_MBEDTLS)
                // mbedtls_dhm_read_params
                mbedtls_ctr_drbg_free(&handshake_.dh.mbedtls_ctr_drbg_);
                mbedtls_entropy_free(&handshake_.dh.mbedtls_entropy_);
                mbedtls_dhm_free(&handshake_.dh.mbedtls_dh_ctx_);
#endif
                break;
            }
            case ::atframe::gw::inner::v1::switch_secret_t_EN_SST_RSA: {
                // TODO init public key and private key
                // mbedtls: use API in mbedtls/pk.h => mbedtls_pk_parse_public_key, mbedtls_pk_parse_keyfile
                // openssl/libressl: use API in openssl/rsa.h,openssl/pem.h => RSA * = PEM_read_RSA_PUBKEY, PEM_read_RSAPrivateKey
                status = error_code_t::EN_ECT_CRYPT_NOT_SUPPORTED;
                break;
            }
            case ::atframe::gw::inner::v1::switch_secret_t_EN_SST_DIRECT: {
                break;
            }
            default: {
                status = error_code_t::EN_ECT_CRYPT_NOT_SUPPORTED;
                break;
            }
            }

            handshake_done(status);
        }

        int libatgw_proto_inner_v1::try_write() {
            if (check_flag(flag_t::EN_PFT_CLOSING)) {
                return error_code_t::EN_ECT_CLOSING;
            }

            if (NULL == callbacks_ || !callbacks_->write_fn) {
                return error_code_t::EN_ECT_MISS_CALLBACKS;
            }

            if (check_flag(flag_t::EN_PFT_WRITING)) {
                return 0;
            }

            // empty then skip write data
            if (write_buffers_.empty()) {
                return 0;
            }

            // first 32bits is hash code, and then 32bits length
            // const size_t msg_header_len = sizeof(uint32_t) + sizeof(uint32_t);

            // closing or closed, cancle writing
            if (check_flag(flag_t::EN_PFT_CLOSING)) {
                while (!write_buffers_.empty()) {
                    ::atbus::detail::buffer_block *bb = write_buffers_.front();
                    size_t nwrite = bb->raw_size();
                    // // nwrite = write_header_offset_ + [data block...]
                    // // data block = 32bits hash+vint+data length
                    // char *buff_start = reinterpret_cast<char *>(bb->raw_data()) + write_header_offset_;
                    // size_t left_length = nwrite - write_header_offset_;
                    // while (left_length >= msg_header_len) {
                    //     // skip 32bits hash
                    //     uint32_t msg_len = flatbuffers::ReadScalar<uint32_t>(buff_start + sizeof(uint32_t));

                    //     // skip 32bits hash and 32bits length
                    //     buff_start += msg_header_len;

                    //     // data length should be enough to hold all data
                    //     if (left_length < msg_header_len + static_cast<size_t>(msg_len)) {
                    //         assert(false);
                    //         left_length = 0;
                    //     }

                    //     callback(UV_ECANCELED, error_code_t::EN_ECT_CLOSING, buff_start, left_length - msg_header_len);

                    //     buff_start += static_cast<size_t>(msg_len);
                    //     // 32bits hash+vint+data length
                    //     left_length -= msg_header_len + static_cast<size_t>(msg_len);
                    // }

                    // remove all cache buffer
                    write_buffers_.pop_front(nwrite, true);
                }

                return error_code_t::EN_ECT_CLOSING;
            }

            int ret = 0;
            bool is_done = false;

            // if not in writing mode, try to merge and write data
            // merge only if message is smaller than read buffer
            if (write_buffers_.limit().cost_number_ > 1 && write_buffers_.front()->raw_size() <= ATFRAME_GATEWAY_MACRO_DATA_SMALL_SIZE) {
                // left write_header_offset_ size at front
                size_t available_bytes = get_tls_length(tls_buffer_t::EN_TBT_MERGE) - write_header_offset_;
                char *buffer_start = reinterpret_cast<char *>(get_tls_buffer(tls_buffer_t::EN_TBT_MERGE));
                char *free_buffer = buffer_start;

                ::atbus::detail::buffer_block *preview_bb = NULL;
                while (!write_buffers_.empty() && available_bytes > 0) {
                    ::atbus::detail::buffer_block *bb = write_buffers_.front();
                    if (bb->raw_size() > available_bytes) {
                        break;
                    }

                    // if write_buffers_ is a static circle buffer, can not merge the bound blocks
                    if (write_buffers_.is_static_mode() && NULL != preview_bb && preview_bb > bb) {
                        break;
                    }
                    preview_bb = bb;

                    // first write_header_offset_ should not be merged, the rest is 32bits hash+varint+len
                    size_t bb_size = bb->raw_size() - write_header_offset_;
                    memcpy(free_buffer, ::atbus::detail::fn::buffer_next(bb->raw_data(), write_header_offset_), bb_size);
                    free_buffer += bb_size;
                    available_bytes -= bb_size;

                    write_buffers_.pop_front(bb->raw_size(), true);
                }

                void *data = NULL;
                write_buffers_.push_front(data, write_header_offset_ + (free_buffer - buffer_start));

                // already pop more data than write_header_offset_ + (free_buffer - buffer_start)
                // so this push_front should always success
                assert(data);
                // at least merge one block
                assert(free_buffer > buffer_start);
                assert(static_cast<size_t>(free_buffer - buffer_start) <=
                       (get_tls_length(tls_buffer_t::EN_TBT_MERGE) - write_header_offset_));

                data = ::atbus::detail::fn::buffer_next(data, write_header_offset_);
                // copy back merged data
                memcpy(data, buffer_start, free_buffer - buffer_start);
            }

            // prepare to writing
            ::atbus::detail::buffer_block *writing_block = write_buffers_.front();

            // should always exist, empty will cause return before
            if (NULL == writing_block) {
                assert(writing_block);
                return error_code_t::EN_ECT_NO_DATA;
            }

            if (writing_block->raw_size() <= write_header_offset_) {
                write_buffers_.pop_front(writing_block->raw_size(), true);
                return try_write();
            }

            // call write
            set_flag(flag_t::EN_PFT_WRITING, true);
            last_write_ptr_ = writing_block->raw_data();
            ret = callbacks_->write_fn(this, writing_block->raw_data(), writing_block->raw_size(), &is_done);
            if (is_done) {
                set_flag(flag_t::EN_PFT_WRITING, false);
            }

            return ret;
        }

        int libatgw_proto_inner_v1::write_msg(flatbuffers::FlatBufferBuilder &builder) {
            // first 32bits is hash code, and then 32bits length
            const size_t msg_header_len = sizeof(uint32_t) + sizeof(uint32_t);

            const void *buf = reinterpret_cast<const void *>(builder.GetBufferPointer());
            size_t len = static_cast<size_t>(builder.GetSize());

            // push back message
            if (NULL != buf && len > 0) {
                if (len >= UINT32_MAX) {
                    return error_code_t::EN_ECT_INVALID_SIZE;
                }

                // get the write block size: write_header_offset_ + header + len）
                size_t total_buffer_size = write_header_offset_ + msg_header_len + len;

                // 判定内存限制
                void *data;
                int res = write_buffers_.push_back(data, total_buffer_size);
                if (res < 0) {
                    return res;
                }

                // skip custom write_header_offset_
                char *buff_start = reinterpret_cast<char *>(data) + write_header_offset_;

                // 32bits hash
                uint32_t hash32 = util::hash::murmur_hash3_x86_32(reinterpret_cast<const char *>(buf), static_cast<int>(len), 0);
                memcpy(buff_start, &hash32, sizeof(uint32_t));

                // length
                flatbuffers::WriteScalar<uint32_t>(buff_start + sizeof(uint32_t), static_cast<uint32_t>(len));
                // buffer
                memcpy(buff_start + msg_header_len, buf, len);
            }

            return try_write();
        }

        int libatgw_proto_inner_v1::write(const void *buffer, size_t len) {
            return send_post(::atframe::gw::inner::v1::cs_msg_type_t_EN_MTT_POST, buffer, len);
        }

        int libatgw_proto_inner_v1::write_done(int status) {
            if (!check_flag(flag_t::EN_PFT_WRITING)) {
                return 0;
            }
            flag_guard_t flag_guard(flags_, flag_t::EN_PFT_IN_CALLBACK);

            void *data = NULL;
            size_t nread, nwrite;

            // first 32bits is hash code, and then 32bits length
            // const size_t msg_header_len = sizeof(uint32_t) + sizeof(uint32_t);

            // popup the lost callback
            while (true) {
                write_buffers_.front(data, nread, nwrite);
                if (NULL == data) {
                    break;
                }

                assert(0 == nread);

                if (0 == nwrite) {
                    write_buffers_.pop_front(0, true);
                    break;
                }

                // nwrite = write_header_offset_ + [data block...]
                // data block = 32bits hash+vint+data length
                // char *buff_start = reinterpret_cast<char *>(data) + write_header_offset_;
                // size_t left_length = nwrite - write_header_offset_;
                // while (left_length >= msg_header_len) {
                //     uint32_t msg_len = flatbuffers::ReadScalar<uint32_t>(buff_start + sizeof(uint32_t));
                //     // skip 32bits hash and 32bits length
                //     buff_start += msg_header_len;

                //     // data length should be enough to hold all data
                //     if (left_length < msg_header_len + static_cast<size_t>(msg_len)) {
                //         assert(false);
                //         left_length = 0;
                //     }

                //     callback(status, last_write_ptr_ == data? 0: TIMEOUT, buff_start, msg_len);

                //     buff_start += static_cast<size_t>(msg_len);

                //     // 32bits hash+32bits length+data length
                //     left_length -= msg_header_len + static_cast<size_t>(msg_len);
                // }

                // remove all cache buffer
                write_buffers_.pop_front(nwrite, true);

                // the end
                if (last_write_ptr_ == data) {
                    break;
                }
            };
            last_write_ptr_ = NULL;

            // unset writing mode
            set_flag(flag_t::EN_PFT_WRITING, false);

            // write left data
            try_write();

            // if in disconnecting status and there is no more data to write, close it
            if (check_flag(flag_t::EN_PFT_CLOSING) && !check_flag(flag_t::EN_PFT_WRITING)) {
                set_flag(flag_t::EN_PFT_CLOSED, true);

                if (NULL != callbacks_ || callbacks_->close_fn) {
                    return callbacks_->close_fn(this, close_reason_);
                }
            }

            return 0;
        }

        int libatgw_proto_inner_v1::close(int reason) { return close(reason, true); }

        int libatgw_proto_inner_v1::close(int reason, bool is_send_kickoff) {
            if (check_flag(flag_t::EN_PFT_CLOSING)) {
                return 0;
            }
            set_flag(flag_t::EN_PFT_CLOSING, true);
            close_reason_ = reason;

            // send kickoff message
            if (is_send_kickoff) {
                send_kickoff(reason);
            }

            // wait writing to finished
            if (!check_flag(flag_t::EN_PFT_WRITING)) {
                set_flag(flag_t::EN_PFT_CLOSED, true);

                if (NULL != callbacks_ || callbacks_->close_fn) {
                    return callbacks_->close_fn(this, close_reason_);
                }
            }

            return 0;
        }

        bool libatgw_proto_inner_v1::check_reconnect(proto_base *other) {
            bool ret = true;
            if (check_flag(flag_t::EN_PFT_CLOSING)) {
                return false;
            }

            libatgw_proto_inner_v1 *other_proto = dynamic_cast<libatgw_proto_inner_v1 *>(other);
            assert(other_proto);

            do {
                if (::atframe::gw::inner::v1::crypt_type_t_EN_ET_NONE == other_proto->crypt_handshake_->type) {
                    ret = true;
                    break;
                }

                // decrypt secret
                const void *outbuf = NULL;
                size_t outsz = 0;
                if (0 !=
                    other_proto->decrypt_data(*other_proto->crypt_handshake_, crypt_handshake_->secret.data(),
                                              crypt_handshake_->secret.size(), outbuf, outsz)) {
                    ret = false;
                    break;
                }

                // compare secret and encrypted secret
                if (NULL == outbuf || outsz != other_proto->crypt_handshake_->secret.size() ||
                    0 != memcmp(outbuf, other_proto->crypt_handshake_->secret.data(), outsz)) {
                    ret = false;
                }
            } while (false);

            // if success, copy crypt information
            if (ret) {
                session_id_ = other_proto->session_id_;
                crypt_read_ = other_proto->crypt_read_;
                crypt_write_ = other_proto->crypt_write_;
                crypt_handshake_ = other_proto->crypt_handshake_;
                handshake_.switch_secret_type = other_proto->handshake_.switch_secret_type;
            }
            return ret;
        }

        void libatgw_proto_inner_v1::set_recv_buffer_limit(size_t max_size, size_t max_number) {
            read_buffers_.set_mode(max_size, max_number);
        }

        void libatgw_proto_inner_v1::set_send_buffer_limit(size_t max_size, size_t max_number) {
            write_buffers_.set_mode(max_size, max_number);
        }

        int libatgw_proto_inner_v1::handshake_update() { return send_key_syn(); }

        int libatgw_proto_inner_v1::start_session() {
            if (check_flag(flag_t::EN_PFT_CLOSING)) {
                return error_code_t::EN_ECT_CLOSING;
            }

            if (NULL == callbacks_ || !callbacks_->write_fn || !crypt_write_) {
                return error_code_t::EN_ECT_MISS_CALLBACKS;
            }

            if (0 != session_id_) {
                return error_code_t::EN_ECT_SESSION_ALREADY_EXIST;
            }

            std::shared_ptr<detail::crypt_global_configure_t> global_cfg = detail::crypt_global_configure_t::current();
            int ret = setup_handshake(global_cfg);
            if (ret < 0) {
                return ret;
            }

            flatbuffers::FlatBufferBuilder builder;
            ::atframe::gw::inner::v1::cs_msgBuilder msg(builder);
            msg.add_head(::atframe::gw::inner::v1::Createcs_msg_head(builder, ::atframe::gw::inner::v1::cs_msg_type_t_EN_MTT_HANDSHAKE,
                                                                     ::atframe::gateway::detail::alloc_seq()));
            ::atframe::gw::inner::v1::cs_body_handshakeBuilder handshake_body(builder);

            handshake_body.add_session_id(0);
            handshake_body.add_step(::atframe::gw::inner::v1::handshake_step_t_EN_HST_START_REQ);
            handshake_body.add_switch_type(::atframe::gw::inner::v1::switch_secret_t_EN_SST_DIRECT);
            handshake_body.add_crypt_type(::atframe::gw::inner::v1::crypt_type_t_EN_ET_NONE);
            handshake_body.add_crypt_bits(0);
            handshake_body.add_crypt_param(builder.CreateVector(reinterpret_cast<const int8_t *>(NULL), 0));

            ::atframe::gw::inner::v1::cs_body_rsa_certBuilder rsa_cert(builder);
            rsa_cert.add_rsa_sign(::atframe::gw::inner::v1::rsa_sign_t_EN_RST_PKCS1);
            rsa_cert.add_hash_type(::atframe::gw::inner::v1::hash_id_t_EN_HIT_MD5);
            rsa_cert.add_pubkey(builder.CreateVector(reinterpret_cast<const int8_t *>(NULL), 0));
            handshake_body.add_rsa_cert(rsa_cert.Finish());

            msg.add_body_type(::atframe::gw::inner::v1::cs_msg_body_cs_body_handshake);
            msg.add_body(handshake_body.Finish().Union());

            builder.Finish(msg.Finish());
            return write_msg(builder);
        }

        int libatgw_proto_inner_v1::reconnect_session(uint64_t sess_id, int type, const std::string &secret, uint32_t keybits) {
            if (check_flag(flag_t::EN_PFT_CLOSING)) {
                return error_code_t::EN_ECT_CLOSING;
            }

            if (NULL == callbacks_ || !callbacks_->write_fn) {
                return error_code_t::EN_ECT_MISS_CALLBACKS;
            }

            if (0 == session_id_) {
                return error_code_t::EN_ECT_SESSION_NOT_FOUND;
            }

            std::shared_ptr<detail::crypt_global_configure_t> global_cfg = detail::crypt_global_configure_t::current();
            int ret = setup_handshake(crypt_handshake_->shared_conf);
            if (ret < 0) {
                return ret;
            }

            // encrypt secrets
            crypt_handshake_->secret = secret;
            crypt_handshake_->setup(type, keybits);

            const void *secret_buffer = NULL;
            size_t secret_length = secret.size();
            encrypt_data(*crypt_handshake_, secret.data(), secret.size(), secret_buffer, secret_length);

            flatbuffers::FlatBufferBuilder builder;
            ::atframe::gw::inner::v1::cs_msgBuilder msg(builder);
            msg.add_head(::atframe::gw::inner::v1::Createcs_msg_head(builder, ::atframe::gw::inner::v1::cs_msg_type_t_EN_MTT_HANDSHAKE,
                                                                     ::atframe::gateway::detail::alloc_seq()));
            ::atframe::gw::inner::v1::cs_body_handshakeBuilder handshake_body(builder);

            handshake_body.add_session_id(sess_id);
            handshake_body.add_step(::atframe::gw::inner::v1::handshake_step_t_EN_HST_RECONNECT_REQ);
            handshake_body.add_switch_type(::atframe::gw::inner::v1::switch_secret_t_EN_SST_DIRECT);
            handshake_body.add_crypt_type(static_cast< ::atframe::gw::inner::v1::crypt_type_t>(type));
            handshake_body.add_crypt_bits(keybits);
            handshake_body.add_crypt_param(builder.CreateVector(reinterpret_cast<const int8_t *>(secret_buffer), secret_length));

            ::atframe::gw::inner::v1::cs_body_rsa_certBuilder rsa_cert(builder);
            rsa_cert.add_rsa_sign(::atframe::gw::inner::v1::rsa_sign_t_EN_RST_PKCS1);
            rsa_cert.add_hash_type(::atframe::gw::inner::v1::hash_id_t_EN_HIT_MD5);
            rsa_cert.add_pubkey(builder.CreateVector(reinterpret_cast<const int8_t *>(secret.data()), secret.size()));
            handshake_body.add_rsa_cert(rsa_cert.Finish());

            msg.add_body_type(::atframe::gw::inner::v1::cs_msg_body_cs_body_handshake);
            msg.add_body(handshake_body.Finish().Union());

            builder.Finish(msg.Finish());
            return write_msg(builder);
        }

        int libatgw_proto_inner_v1::send_post(::atframe::gw::inner::v1::cs_msg_type_t msg_type, const void *buffer, size_t len) {
            if (check_flag(flag_t::EN_PFT_CLOSING)) {
                return error_code_t::EN_ECT_CLOSING;
            }

            if (NULL == callbacks_ || !callbacks_->write_fn || !crypt_write_) {
                return error_code_t::EN_ECT_MISS_CALLBACKS;
            }

            // encrypt/zip
            size_t ori_len = len;
            int res = encode_post(buffer, len, buffer, len);
            if (0 != res) {
                return res;
            }

            // pack
            flatbuffers::FlatBufferBuilder builder;
            ::atframe::gw::inner::v1::cs_msgBuilder msg(builder);
            msg.add_head(::atframe::gw::inner::v1::Createcs_msg_head(builder, msg_type, ::atframe::gateway::detail::alloc_seq()));
            ::atframe::gw::inner::v1::cs_body_postBuilder post_body(builder);

            post_body.add_length(static_cast<uint64_t>(ori_len));
            post_body.add_data(builder.CreateVector(reinterpret_cast<const int8_t *>(buffer), len));

            msg.add_body_type(::atframe::gw::inner::v1::cs_msg_body_cs_body_post);
            msg.add_body(post_body.Finish().Union());

            builder.Finish(msg.Finish());
            return write_msg(builder);
        }

        int libatgw_proto_inner_v1::send_ping(time_t tp) {
            if (check_flag(flag_t::EN_PFT_CLOSING)) {
                return error_code_t::EN_ECT_CLOSING;
            }

            if (NULL == callbacks_ || !callbacks_->write_fn) {
                return error_code_t::EN_ECT_MISS_CALLBACKS;
            }

            ping_.last_ping = tp;

            flatbuffers::FlatBufferBuilder builder;
            ::atframe::gw::inner::v1::cs_msgBuilder msg(builder);
            msg.add_head(::atframe::gw::inner::v1::Createcs_msg_head(builder, ::atframe::gw::inner::v1::cs_msg_type_t_EN_MTT_PING,
                                                                     ::atframe::gateway::detail::alloc_seq()));
            ::atframe::gw::inner::v1::cs_body_pingBuilder ping_body(builder);

            ping_body.add_timepoint(static_cast<int64_t>(tp));

            msg.add_body_type(::atframe::gw::inner::v1::cs_msg_body_cs_body_ping);
            msg.add_body(ping_body.Finish().Union());

            builder.Finish(msg.Finish());
            return write_msg(builder);
        }

        int libatgw_proto_inner_v1::send_pong(time_t tp) {
            if (check_flag(flag_t::EN_PFT_CLOSING)) {
                return error_code_t::EN_ECT_CLOSING;
            }

            if (NULL == callbacks_ || !callbacks_->write_fn) {
                return error_code_t::EN_ECT_MISS_CALLBACKS;
            }

            flatbuffers::FlatBufferBuilder builder;
            ::atframe::gw::inner::v1::cs_msgBuilder msg(builder);
            msg.add_head(::atframe::gw::inner::v1::Createcs_msg_head(builder, ::atframe::gw::inner::v1::cs_msg_type_t_EN_MTT_PONG,
                                                                     ::atframe::gateway::detail::alloc_seq()));
            ::atframe::gw::inner::v1::cs_body_pingBuilder ping_body(builder);

            ping_body.add_timepoint(static_cast<int64_t>(tp));

            msg.add_body_type(::atframe::gw::inner::v1::cs_msg_body_cs_body_ping);
            msg.add_body(ping_body.Finish().Union());

            builder.Finish(msg.Finish());
            return write_msg(builder);
        }

        int libatgw_proto_inner_v1::send_key_syn() {
            if (check_flag(flag_t::EN_PFT_CLOSING)) {
                return error_code_t::EN_ECT_CLOSING;
            }

            if (NULL == callbacks_ || !callbacks_->write_fn || !crypt_write_) {
                return error_code_t::EN_ECT_MISS_CALLBACKS;
            }

            // if handshake is running, do not start handshake again.
            if (handshake_.has_data) {
                return 0;
            }

            // make a new crypt session for handshake
            crypt_handshake_ = std::make_shared<crypt_session_t>();

            // and then, just like start rsp
            std::shared_ptr<detail::crypt_global_configure_t> global_cfg = detail::crypt_global_configure_t::current();
            int ret = setup_handshake(global_cfg);
            if (ret < 0) {
                return ret;
            }

            flatbuffers::FlatBufferBuilder builder;
            ::atframe::gw::inner::v1::cs_msgBuilder msg(builder);
            msg.add_head(::atframe::gw::inner::v1::Createcs_msg_head(builder, ::atframe::gw::inner::v1::cs_msg_type_t_EN_MTT_POST_KEY_SYN,
                                                                     ::atframe::gateway::detail::alloc_seq()));
            ::atframe::gw::inner::v1::cs_body_handshakeBuilder handshake_body(builder);

            ret = pack_handshake_start_rsp(builder, session_id_, handshake_body);
            if (ret < 0) {
                return ret;
            }

            msg.add_body_type(::atframe::gw::inner::v1::cs_msg_body_cs_body_handshake);
            msg.add_body(handshake_body.Finish().Union());

            builder.Finish(msg.Finish());
            ret = write_msg(builder);
            return ret;
        }

        int libatgw_proto_inner_v1::send_kickoff(int reason) {
            if (check_flag(flag_t::EN_PFT_CLOSING)) {
                return error_code_t::EN_ECT_CLOSING;
            }

            if (NULL == callbacks_ || !callbacks_->write_fn) {
                return error_code_t::EN_ECT_MISS_CALLBACKS;
            }

            flatbuffers::FlatBufferBuilder builder;
            ::atframe::gw::inner::v1::cs_msgBuilder msg(builder);
            msg.add_head(::atframe::gw::inner::v1::Createcs_msg_head(builder, ::atframe::gw::inner::v1::cs_msg_type_t_EN_MTT_KICKOFF,
                                                                     ::atframe::gateway::detail::alloc_seq()));
            ::atframe::gw::inner::v1::cs_body_kickoffBuilder kickoff_body(builder);

            kickoff_body.add_reason(reason);

            msg.add_body_type(::atframe::gw::inner::v1::cs_msg_body_cs_body_kickoff);
            msg.add_body(kickoff_body.Finish().Union());

            builder.Finish(msg.Finish());
            return write_msg(builder);
        }

        int libatgw_proto_inner_v1::send_verify(const void *buf, size_t sz) {
            if (check_flag(flag_t::EN_PFT_CLOSING)) {
                return error_code_t::EN_ECT_CLOSING;
            }

            if (NULL == callbacks_ || !callbacks_->write_fn || !crypt_write_) {
                return error_code_t::EN_ECT_MISS_CALLBACKS;
            }

            // pack
            flatbuffers::FlatBufferBuilder builder;
            ::atframe::gw::inner::v1::cs_msgBuilder msg(builder);
            msg.add_head(::atframe::gw::inner::v1::Createcs_msg_head(builder, ::atframe::gw::inner::v1::cs_msg_type_t_EN_MTT_HANDSHAKE,
                                                                     ::atframe::gateway::detail::alloc_seq()));
            ::atframe::gw::inner::v1::cs_body_handshakeBuilder verify_body(builder);

            verify_body.add_session_id(session_id_);
            verify_body.add_step(::atframe::gw::inner::v1::handshake_step_t_EN_HST_VERIFY);

            // copy data
            verify_body.add_switch_type(static_cast< ::atframe::gw::inner::v1::switch_secret_t>(handshake_.switch_secret_type));
            verify_body.add_crypt_type(static_cast< ::atframe::gw::inner::v1::crypt_type_t>(crypt_write_->type));
            verify_body.add_crypt_bits(crypt_write_->keybits);

            const void *outbuf = NULL;
            size_t outsz = 0;
            int ret = 0;
            if (NULL != buf && sz > 0) {
                ret = encrypt_data(*crypt_write_, buf, sz, outbuf, outsz);
            }

            if (0 == ret) {
                verify_body.add_crypt_param(builder.CreateVector(reinterpret_cast<const int8_t *>(outbuf), outsz));
            } else {
                verify_body.add_session_id(0);
                verify_body.add_crypt_param(builder.CreateVector(reinterpret_cast<const int8_t *>(NULL), 0));
            }

            msg.add_body_type(::atframe::gw::inner::v1::cs_msg_body_cs_body_handshake);
            msg.add_body(verify_body.Finish().Union());

            builder.Finish(msg.Finish());
            return write_msg(builder);
        }

        int libatgw_proto_inner_v1::encode_post(const void *in, size_t insz, const void *&out, size_t &outsz) {
            if (check_flag(flag_t::EN_PFT_CLOSING)) {
                outsz = insz;
                out = in;
                return error_code_t::EN_ECT_CLOSING;
            }

            // encrypt
            if (!crypt_write_) {
                outsz = insz;
                out = in;
                return error_code_t::EN_ECT_HANDSHAKE;
            }
            int ret = encrypt_data(*crypt_write_, in, insz, out, outsz);
            // if (0 != ret) {
            //     return ret;
            // }

            // TODO zip
            return ret;
        }

        int libatgw_proto_inner_v1::decode_post(const void *in, size_t insz, const void *&out, size_t &outsz) {
            if (check_flag(flag_t::EN_PFT_CLOSING)) {
                outsz = insz;
                out = in;
                return error_code_t::EN_ECT_CLOSING;
            }

            // TODO unzip
            // decrypt
            if (!crypt_read_) {
                outsz = insz;
                out = in;
                return error_code_t::EN_ECT_HANDSHAKE;
            }
            return decrypt_data(*crypt_read_, in, insz, out, outsz);
        }

        int libatgw_proto_inner_v1::encrypt_data(crypt_session_t &crypt_info, const void *in, size_t insz, const void *&out,
                                                 size_t &outsz) {
            if (check_flag(flag_t::EN_PFT_CLOSING)) {
                return error_code_t::EN_ECT_CLOSING;
            }

            if (0 == insz || NULL == in) {
                out = in;
                outsz = insz;
                return error_code_t::EN_ECT_PARAM;
            }

            int ret = 0;
            void *buffer = get_tls_buffer(tls_buffer_t::EN_TBT_CRYPT);
            size_t len = get_tls_length(tls_buffer_t::EN_TBT_CRYPT);

            switch (crypt_info.type) {
            case ::atframe::gw::inner::v1::crypt_type_t_EN_ET_XXTEA: {
                size_t outsz = ((insz - 1) | 0x03) + 1;

                if (len < outsz) {
                    return error_code_t::EN_ECT_MSG_TOO_LARGE;
                }

                memcpy(buffer, in, insz);
                if (outsz > insz) {
                    memset(reinterpret_cast<char *>(buffer) + insz, 0, outsz - insz);
                }

                ::util::xxtea_encrypt(&crypt_info.xtea_key.util_xxtea_ctx, buffer, outsz);
                out = buffer;
                break;
            }
            case ::atframe::gw::inner::v1::crypt_type_t_EN_ET_AES: {
                unsigned char iv[AES_BLOCK_SIZE];
                memset(iv, 0, sizeof(iv));

                size_t outsz = ((insz - 1) | (AES_BLOCK_SIZE - 1)) + 1;

                if (len < outsz) {
                    return error_code_t::EN_ECT_MSG_TOO_LARGE;
                }

                memcpy(buffer, in, insz);
                if (outsz > insz) {
                    memset(reinterpret_cast<char *>(buffer) + insz, 0, outsz - insz);
                }

#if defined(LIBATFRAME_ATGATEWAY_ENABLE_OPENSSL) || defined(LIBATFRAME_ATGATEWAY_ENABLE_LIBRESSL)
                AES_cbc_encrypt(reinterpret_cast<const unsigned char *>(buffer), reinterpret_cast<unsigned char *>(buffer), outsz,
                                &crypt_info.aes_key.openssl_decrypt_key, iv, AES_ENCRYPT);

#elif defined(LIBATFRAME_ATGATEWAY_ENABLE_MBEDTLS)
                ret = mbedtls_aes_crypt_cbc(&crypt_info.aes_key.mbedtls_aes_encrypt_ctx, MBEDTLS_AES_ENCRYPT, outsz, iv,
                                            reinterpret_cast<const unsigned char *>(buffer), reinterpret_cast<unsigned char *>(buffer));
                if (ret < 0) {
                    ATFRAME_GATEWAY_ON_ERROR(ret, "mbedtls AES encrypt failed");
                    return ret;
                }
#endif
                break;
            }
            default: {
                out = in;
                outsz = insz;
                break;
            }
            }

            return ret;
        }

        int libatgw_proto_inner_v1::decrypt_data(crypt_session_t &crypt_info, const void *in, size_t insz, const void *&out,
                                                 size_t &outsz) {
            if (check_flag(flag_t::EN_PFT_CLOSING)) {
                return error_code_t::EN_ECT_CLOSING;
            }

            if (0 == insz || NULL == in) {
                out = in;
                outsz = insz;
                return error_code_t::EN_ECT_PARAM;
            }

            int ret = 0;
            void *buffer = get_tls_buffer(tls_buffer_t::EN_TBT_CRYPT);
            size_t len = get_tls_length(tls_buffer_t::EN_TBT_CRYPT);

            switch (crypt_info.type) {
            case ::atframe::gw::inner::v1::crypt_type_t_EN_ET_XXTEA: {
                size_t outsz = ((insz - 1) | 0x03) + 1;

                if (len < outsz) {
                    return error_code_t::EN_ECT_MSG_TOO_LARGE;
                }

                memcpy(buffer, in, insz);
                if (outsz > insz) {
                    memset(reinterpret_cast<char *>(buffer) + insz, 0, outsz - insz);
                }

                ::util::xxtea_decrypt(&crypt_info.xtea_key.util_xxtea_ctx, buffer, outsz);
                out = buffer;
                break;
            }
            case ::atframe::gw::inner::v1::crypt_type_t_EN_ET_AES: {
                unsigned char iv[AES_BLOCK_SIZE];
                memset(iv, 0, sizeof(iv));

                size_t outsz = ((insz - 1) | (AES_BLOCK_SIZE - 1)) + 1;

                if (len < outsz) {
                    return error_code_t::EN_ECT_MSG_TOO_LARGE;
                }

                memcpy(buffer, in, insz);
                if (outsz > insz) {
                    memset(reinterpret_cast<char *>(buffer) + insz, 0, outsz - insz);
                }

#if defined(LIBATFRAME_ATGATEWAY_ENABLE_OPENSSL) || defined(LIBATFRAME_ATGATEWAY_ENABLE_LIBRESSL)
                AES_cbc_encrypt(reinterpret_cast<const unsigned char *>(buffer), reinterpret_cast<unsigned char *>(buffer), outsz,
                                &crypt_info.aes_key.openssl_decrypt_key, iv, AES_DECRYPT);

#elif defined(LIBATFRAME_ATGATEWAY_ENABLE_MBEDTLS)
                ret = mbedtls_aes_crypt_cbc(&crypt_info.aes_key.mbedtls_aes_decrypt_ctx, MBEDTLS_AES_DECRYPT, outsz, iv,
                                            reinterpret_cast<const unsigned char *>(buffer), reinterpret_cast<unsigned char *>(buffer));
                if (ret < 0) {
                    ATFRAME_GATEWAY_ON_ERROR(ret, "mbedtls AES decrypt failed");
                    return ret;
                }
#endif
                break;
            }
            default: {
                out = in;
                outsz = insz;
                break;
            }
            }

            return ret;
        }

        int libatgw_proto_inner_v1::global_reload(crypt_conf_t &crypt_conf) {
            // spin_lock
            static ::util::lock::spin_lock global_proto_lock;
            ::util::lock::lock_holder< ::util::lock::spin_lock> lh(global_proto_lock);

            detail::crypt_global_configure_t::ptr_t inst = std::make_shared<detail::crypt_global_configure_t>(crypt_conf);
            if (!inst) {
                return error_code_t::EN_ECT_MALLOC;
            }

            int ret = inst->init();
            if (0 == ret) {
                detail::crypt_global_configure_t::current().swap(inst);
            }

            return ret;
        }
    }
}