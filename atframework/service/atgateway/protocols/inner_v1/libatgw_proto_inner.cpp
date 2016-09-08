#include <sstream>

#include "common/string_oprs.h"
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

#if defined(LIBATFRAME_ATGATEWAY_ENABLE_OPENSSL) || defined(LIBATFRAME_ATGATEWAY_ENABLE_LIBRESSL)

// copy from ssl_locl.h
#ifndef n2s
# define n2s(c,s)        ((s=(((unsigned int)(c[0]))<< 8)| \
                            (((unsigned int)(c[1]))    )),c+=2)
#endif

// copy from ssl_locl.h
#ifndef s2n
# define s2n(s,c)        ((c[0]=(unsigned char)(((s)>> 8)&0xff), \
                          c[1]=(unsigned char)(((s)    )&0xff)),c+=2)
#endif

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

// init random engine
#if defined(LIBATFRAME_ATGATEWAY_ENABLE_OPENSSL) || defined(LIBATFRAME_ATGATEWAY_ENABLE_LIBRESSL)
#elif defined(LIBATFRAME_ATGATEWAY_ENABLE_MBEDTLS)
                    mbedtls_ctr_drbg_init(&mbedtls_ctr_drbg_);
                    mbedtls_entropy_init(&mbedtls_entropy_);

                    ret = mbedtls_ctr_drbg_seed(&mbedtls_ctr_drbg_, mbedtls_entropy_func, &mbedtls_entropy_, NULL, 0);
                    if (0 != ret) {
                        return error_code_t::EN_ECT_CRYPT_INIT_DHPARAM;
                    }
#endif

                    if (::atframe::gw::inner::v1::crypt_type_t_EN_ET_NONE == conf_.type) {
                        inited_ = true;
                        return ret;
                    }

                    switch (conf_.switch_secret_type) {
                    case ::atframe::gw::inner::v1::switch_secret_t_EN_SST_DH: {
                        // do nothing in client mode
                        if (conf_.client_mode) {
                            break;
                        }

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

                            // check 
                            DH* test_dh_ctx = PEM_read_bio_DHparams(openssl_dh_bio_, NULL, NULL, NULL);
                            if (NULL == test_dh_ctx) {
                                ret = error_code_t::EN_ECT_CRYPT_READ_DHPARAM_FILE;
                            } else {
                                int errcode = 0;
                                DH_check(test_dh_ctx, &errcode);
                                if (((DH_CHECK_P_NOT_SAFE_PRIME | DH_NOT_SUITABLE_GENERATOR | DH_UNABLE_TO_CHECK_GENERATOR) & errcode)) {
                                    ret = error_code_t::EN_ECT_CRYPT_READ_DHPARAM_FILE;
                                }
                                DH_free(test_dh_ctx);
                            }
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
                        } else {
                            mbedtls_dhm_free(&test_dh_ctx);
                        }

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
                        // do nothing in client mode
                        if (conf_.client_mode) {
                            break;
                        }
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

// close random engine
#if defined(LIBATFRAME_ATGATEWAY_ENABLE_OPENSSL) || defined(LIBATFRAME_ATGATEWAY_ENABLE_LIBRESSL)
#elif defined(LIBATFRAME_ATGATEWAY_ENABLE_MBEDTLS)
                    mbedtls_ctr_drbg_free(&mbedtls_ctr_drbg_);
                    mbedtls_entropy_free(&mbedtls_entropy_);
#endif
                }

                static void default_crypt_configure(libatgw_proto_inner_v1::crypt_conf_t& dconf) {
                    dconf.default_key = "atgw-key";
                    dconf.dh_param.clear();
                    dconf.keybits = 128;
                    dconf.switch_secret_type = ::atframe::gw::inner::v1::switch_secret_t_EN_SST_DIRECT;
                    dconf.type = ::atframe::gw::inner::v1::crypt_type_t_EN_ET_NONE;
                    dconf.update_interval = 1200;
                    dconf.client_mode = false;
                }

                libatgw_proto_inner_v1::crypt_conf_t conf_;
                bool inited_;

#if defined(LIBATFRAME_ATGATEWAY_ENABLE_OPENSSL) || defined(LIBATFRAME_ATGATEWAY_ENABLE_LIBRESSL)
                BIO *openssl_dh_bio_;
#elif defined(LIBATFRAME_ATGATEWAY_ENABLE_MBEDTLS)
                // move mbedtls_ctr_drbg_context and mbedtls_entropy_context here
                std::string mbedtls_dh_param_;
                mbedtls_ctr_drbg_context mbedtls_ctr_drbg_;
                mbedtls_entropy_context mbedtls_entropy_;
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

                ::util::xxtea_setup(&xtea_key.util_xxtea_ctx, reinterpret_cast<const unsigned char *>(secret.data()));
                break;
            }
            case ::atframe::gw::inner::v1::crypt_type_t_EN_ET_AES: {
                if (secret.size() * 8 < kb) {
                    return error_code_t::EN_ECT_CRYPT_NOT_SUPPORTED;
                }

#if defined(LIBATFRAME_ATGATEWAY_ENABLE_OPENSSL) || defined(LIBATFRAME_ATGATEWAY_ENABLE_LIBRESSL)
                int res = AES_set_encrypt_key(reinterpret_cast<const unsigned char *>(secret.data()), kb, &aes_key.openssl_encrypt_key);
                if (res < 0) {
                    return res;
                }

                res = AES_set_decrypt_key(reinterpret_cast<const unsigned char *>(secret.data()), kb, &aes_key.openssl_decrypt_key);
                if (res < 0) {
                    return res;
                }

#elif defined(LIBATFRAME_ATGATEWAY_ENABLE_MBEDTLS)
                mbedtls_aes_init(&aes_key.mbedtls_aes_encrypt_ctx);
                mbedtls_aes_init(&aes_key.mbedtls_aes_decrypt_ctx);
                int res =
                    mbedtls_aes_setkey_enc(&aes_key.mbedtls_aes_encrypt_ctx, reinterpret_cast<const unsigned char *>(secret.data()), kb);
                if (res < 0) {
                    return res;
                }

                res = mbedtls_aes_setkey_dec(&aes_key.mbedtls_aes_decrypt_ctx, reinterpret_cast<const unsigned char *>(secret.data()), kb);
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

            ping_.last_ping = ping_data_t::clk_t::from_time_t(0);
            ping_.last_delta = 0;

            handshake_.switch_secret_type = 0;
            handshake_.has_data = false;
            handshake_.ext_data = NULL;
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
            case atframe::gw::inner::v1::cs_msg_type_t_EN_MTT_POST_KEY_SYN:
            case atframe::gw::inner::v1::cs_msg_type_t_EN_MTT_POST_KEY_ACK: {
                if (::atframe::gw::inner::v1::cs_msg_body_cs_body_handshake != msg->body_type()) {
                    close(close_reason_t::EN_CRT_INVALID_DATA, false);
                    break;
                }
                const ::atframe::gw::inner::v1::cs_body_handshake *msg_body =
                    static_cast<const ::atframe::gw::inner::v1::cs_body_handshake *>(msg->body());

                // start to update handshake
                if (!check_flag(flag_t::EN_PFT_HANDSHAKE_UPDATE)) {
                    set_flag(flag_t::EN_PFT_HANDSHAKE_UPDATE, true);
                }

                dispatch_handshake(*msg_body);
                break;
            }
            case atframe::gw::inner::v1::cs_msg_type_t_EN_MTT_HANDSHAKE: {
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
                send_pong(msg_body->timepoint());
                break;
            }
            case atframe::gw::inner::v1::cs_msg_type_t_EN_MTT_PONG: {
                if (::atframe::gw::inner::v1::cs_msg_body_cs_body_ping != msg->body_type()) {
                    close(close_reason_t::EN_CRT_INVALID_DATA);
                    break;
                }

                // update ping/pong duration
                if (0 != ping_data_t::clk_t::to_time_t(ping_.last_ping)) {
                    ping_.last_delta = static_cast<time_t>(std::chrono::duration_cast<std::chrono::milliseconds>(ping_data_t::clk_t::now() - ping_.last_ping).count());
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

            callbacks_->new_session_fn(this, session_id_);

            using namespace ::atframe::gw::inner::v1;

            flatbuffers::FlatBufferBuilder builder;
            flatbuffers::Offset<cs_msg_head> header_data = Createcs_msg_head(builder, cs_msg_type_t_EN_MTT_HANDSHAKE,
                                                                     ::atframe::gateway::detail::alloc_seq());
            flatbuffers::Offset<cs_body_handshake> handshake_body;
            ret = pack_handshake_start_rsp(builder, session_id_, handshake_body);
            if (ret < 0) {
                handshake_done(ret);
                return ret;
            }

            builder.Finish(Createcs_msg(builder, header_data, cs_msg_body_cs_body_handshake, handshake_body.Union()), cs_msgIdentifier());
            ret = write_msg(builder);
            if (ret < 0) {
                handshake_done(ret);
            }
            return ret;
        }

        int libatgw_proto_inner_v1::dispatch_handshake_start_rsp(const ::atframe::gw::inner::v1::cs_body_handshake &body_handshake) {
            // check switch type
            // check if start new session success
            if (0 == body_handshake.session_id()) {
                ATFRAME_GATEWAY_ON_ERROR(error_code_t::EN_ECT_HANDSHAKE, "start new session refused.");
                return error_code_t::EN_ECT_HANDSHAKE;
            }

            // assign session id,
            session_id_ = body_handshake.session_id();

            // if is running handshake, can not handshake again
            if (!handshake_.has_data) {
                // make a new crypt session for handshake
                if (crypt_handshake_->shared_conf) {
                    crypt_handshake_ = std::make_shared<crypt_session_t>();
                }


                std::shared_ptr<detail::crypt_global_configure_t> global_cfg = detail::crypt_global_configure_t::current();
                // check if global configure changed
                if (!global_cfg || global_cfg->conf_.type != body_handshake.crypt_type() ||
                    global_cfg->conf_.switch_secret_type != body_handshake.switch_type() ||
                    global_cfg->conf_.keybits != body_handshake.crypt_bits()) {
                    crypt_conf_t global_crypt_cfg;
                    detail::crypt_global_configure_t::default_crypt_configure(global_crypt_cfg);
                    global_crypt_cfg.type = body_handshake.crypt_type();
                    global_crypt_cfg.switch_secret_type = body_handshake.switch_type();
                    global_crypt_cfg.keybits = body_handshake.crypt_bits();
                    global_crypt_cfg.client_mode = true;
                    ::atframe::gateway::libatgw_proto_inner_v1::global_reload(global_crypt_cfg);
                    global_cfg = detail::crypt_global_configure_t::current();
                }
                int ret = setup_handshake(global_cfg);
                if (ret < 0) {
                    return ret;
                }
            } else {
                return error_code_t::EN_ECT_HANDSHAKE;
            }

            // if not use crypt, assign crypt information and close_handshake(0)
            if (::atframe::gw::inner::v1::crypt_type_t_EN_ET_NONE == body_handshake.crypt_type()) {
                crypt_handshake_->setup(::atframe::gw::inner::v1::crypt_type_t_EN_ET_NONE, 0);
                crypt_read_ = crypt_handshake_;
                crypt_write_ = crypt_handshake_;
                close_handshake(0);
                return 0;
            }

            handshake_.switch_secret_type = body_handshake.switch_type();
            if (NULL == body_handshake.crypt_param()) {
                ATFRAME_GATEWAY_ON_ERROR(error_code_t::EN_ECT_HANDSHAKE, "has no secret");
                return error_code_t::EN_ECT_HANDSHAKE;
            }

            int ret = 0;
            switch (handshake_.switch_secret_type) {
            case ::atframe::gw::inner::v1::switch_secret_t_EN_SST_DIRECT: {
                crypt_handshake_->secret.resize(body_handshake.crypt_param()->size());
                memcpy(crypt_handshake_->secret.data(), body_handshake.crypt_param()->data(), body_handshake.crypt_param()->size());

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
                using namespace ::atframe::gw::inner::v1;

                flatbuffers::FlatBufferBuilder builder;
                flatbuffers::Offset<cs_msg_head> header_data = Createcs_msg_head(builder, cs_msg_type_t_EN_MTT_HANDSHAKE,
                    ::atframe::gateway::detail::alloc_seq());
                flatbuffers::Offset<cs_body_handshake> handshake_body;

                ret = pack_handshake_dh_pubkey_req(builder, body_handshake, handshake_body);
                if (ret < 0) {
                    break;
                }

                builder.Finish(Createcs_msg(builder, header_data, cs_msg_body_cs_body_handshake, handshake_body.Union()), cs_msgIdentifier());
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
            handshake_.ext_data = &body_handshake;

            int ret = callbacks_->reconnect_fn(this, body_handshake.session_id());
            // after this , can not failed any more, because session had already accepted.

            using namespace ::atframe::gw::inner::v1;

            flatbuffers::FlatBufferBuilder builder;
            flatbuffers::Offset<cs_msg_head> header_data = Createcs_msg_head(builder, cs_msg_type_t_EN_MTT_HANDSHAKE,
                ::atframe::gateway::detail::alloc_seq());
            flatbuffers::Offset<cs_body_handshake> reconn_body;

            uint64_t sess_id = 0;
            if (0 == ret) {
                sess_id = session_id_;
            }

            reconn_body = Createcs_body_handshake(builder,
                sess_id, handshake_step_t_EN_HST_RECONNECT_RSP,
                static_cast<::atframe::gw::inner::v1::switch_secret_t>(handshake_.switch_secret_type),
                static_cast< ::atframe::gw::inner::v1::crypt_type_t>(crypt_handshake_->type),
                crypt_handshake_->keybits
            );

            builder.Finish(Createcs_msg(builder, header_data, cs_msg_body_cs_body_handshake, reconn_body.Union()), cs_msgIdentifier());

            crypt_read_ = crypt_handshake_;
            crypt_write_ = crypt_handshake_;

            if (0 != ret) {
                write_msg(builder);
                close_handshake(ret);
                close(ret, true);
            } else {
                
                ret = write_msg(builder);
                close_handshake(ret);

                // change key immediately, in case of Man-in-the-Middle Attack
                ret = handshake_update();
                if (0 != ret) {
                    ATFRAME_GATEWAY_ON_ERROR(ret, "reconnect to old session refused.");
                }
            }

            return ret;
        }

        int libatgw_proto_inner_v1::dispatch_handshake_reconn_rsp(const ::atframe::gw::inner::v1::cs_body_handshake &body_handshake) {
            // if success, session id is not 0, and assign all data
            if (0 == body_handshake.session_id()) {
                ATFRAME_GATEWAY_ON_ERROR(error_code_t::EN_ECT_REFUSE_RECONNECT, "update handshake failed.");

                // force to trigger handshake done
                setup_handshake(crypt_handshake_->shared_conf);
                close_handshake(error_code_t::EN_ECT_REFUSE_RECONNECT);
                return error_code_t::EN_ECT_HANDSHAKE;
            }

            std::shared_ptr<detail::crypt_global_configure_t> global_cfg = detail::crypt_global_configure_t::current();
            // check if global configure changed
            if (!global_cfg || global_cfg->conf_.type != body_handshake.crypt_type() ||
                global_cfg->conf_.switch_secret_type != body_handshake.switch_type() ||
                global_cfg->conf_.keybits != body_handshake.crypt_bits()) {
                crypt_conf_t global_crypt_cfg;
                detail::crypt_global_configure_t::default_crypt_configure(global_crypt_cfg);
                global_crypt_cfg.type = body_handshake.crypt_type();
                global_crypt_cfg.switch_secret_type = body_handshake.switch_type();
                global_crypt_cfg.keybits = body_handshake.crypt_bits();
                global_crypt_cfg.client_mode = true;
                ::atframe::gateway::libatgw_proto_inner_v1::global_reload(global_crypt_cfg);
                global_cfg = detail::crypt_global_configure_t::current();
            }
            int ret = setup_handshake(global_cfg);
            if (ret < 0) {
                return ret;
            }

            session_id_ = body_handshake.session_id();
            crypt_read_ = crypt_handshake_;
            crypt_write_ = crypt_handshake_;

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

            using namespace ::atframe::gw::inner::v1;
            flatbuffers::FlatBufferBuilder builder;
            flatbuffers::Offset<cs_msg_head> header_data = Createcs_msg_head(builder, cs_msg_type_t_EN_MTT_HANDSHAKE,
                ::atframe::gateway::detail::alloc_seq());
            flatbuffers::Offset<cs_body_handshake> pubkey_rsp_body;


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

                crypt_handshake_->setup(crypt_handshake_->shared_conf->conf_.type, crypt_handshake_->shared_conf->conf_.keybits);
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
                res = mbedtls_dhm_calc_secret(&handshake_.dh.mbedtls_dh_ctx_,
                                              reinterpret_cast<unsigned char *>(&crypt_handshake_->secret[0]), psz, &psz, NULL, NULL);
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
                // generate a verify prefix
#if defined(LIBATFRAME_ATGATEWAY_ENABLE_OPENSSL) || defined(LIBATFRAME_ATGATEWAY_ENABLE_LIBRESSL)
                BIGNUM *rnd_vfy = BN_new();
                if (NULL != rnd_vfy) {
                    if (1 == BN_rand(rnd_vfy, static_cast<int>(crypt_handshake_->shared_conf->conf_.keybits), 0, 0)) {
                        char * verify_text = BN_bn2hex(rnd_vfy);
                        size_t verify_text_len = strlen(verify_text);
                        if (NULL != verify_text) {
                            ret = encrypt_data(*crypt_handshake_, verify_text, verify_text_len, outbuf, outsz);
                            crypt_handshake_->param.assign(verify_text, verify_text + verify_text_len);

                            OPENSSL_free(verify_text);
                        }
                    } else {
                        ATFRAME_GATEWAY_ON_ERROR(static_cast<int>(ERR_get_error()), "openssl/libressl generate verify text failed");
                    }
                    BN_free(rnd_vfy);
                }
                
#elif defined(LIBATFRAME_ATGATEWAY_ENABLE_MBEDTLS)
                {
                    size_t secret_len = crypt_handshake_->shared_conf->conf_.keybits / 8;
                    // 3 * secret_len, 1 for binary data, 2 for hex data
                    unsigned char* verify_text = (unsigned char*)malloc((secret_len << 1) + secret_len);
                    if (NULL != verify_text) {
                        int res = mbedtls_ctr_drbg_random(&crypt_handshake_->shared_conf->mbedtls_ctr_drbg_,
                            verify_text, secret_len);
                        if (0 == res) {
                            util::string::dumphex(verify_text, secret_len, verify_text + secret_len);
                            ret = encrypt_data(*crypt_handshake_, verify_text + secret_len, secret_len << 1, outbuf, outsz);
                        } else {
                            ATFRAME_GATEWAY_ON_ERROR(res, "mbedtls generate verify text failed");
                        }
                        crypt_handshake_->param.assign(verify_text, verify_text + (secret_len<< 1));
                        free(verify_text);
                    }
                }
#endif

                if (NULL == outbuf || 0 == outsz) {
                    ret = encrypt_data(*crypt_handshake_, crypt_handshake_->shared_conf->conf_.default_key.data(),
                        crypt_handshake_->shared_conf->conf_.default_key.size(), outbuf, outsz);
                }
            }

            if (0 == ret) {
                pubkey_rsp_body = Createcs_body_handshake(builder, session_id_, handshake_step_t_EN_HST_DH_PUBKEY_RSP,
                    peer_body.switch_type(), peer_body.crypt_type(), peer_body.crypt_bits(),
                    builder.CreateVector(reinterpret_cast<const int8_t *>(outbuf), NULL == outbuf? 0: outsz)
                );
            } else {
                pubkey_rsp_body = Createcs_body_handshake(builder, 0, handshake_step_t_EN_HST_DH_PUBKEY_RSP,
                    peer_body.switch_type(), peer_body.crypt_type(), peer_body.crypt_bits(),
                    builder.CreateVector(reinterpret_cast<const int8_t *>(NULL), 0)
                );
            }

            builder.Finish(Createcs_msg(builder, header_data, cs_msg_body_cs_body_handshake, pubkey_rsp_body.Union()), cs_msgIdentifier());
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
                    verify_data.reserve(outsz + outsz);
                    verify_data.assign(reinterpret_cast<const char *>(outbuf), outsz);

                    // append something, verify encrypt/decript
#if defined(LIBATFRAME_ATGATEWAY_ENABLE_OPENSSL) || defined(LIBATFRAME_ATGATEWAY_ENABLE_LIBRESSL)
                    BIGNUM *rnd_vfy = BN_new();
                    if (NULL != rnd_vfy) {
                        // hex size = outsz, binary size = outsz / 2, bits = outsz / 2 * 8 = outsz * 4
                        if (1 == BN_rand(rnd_vfy, static_cast<int>(outsz<< 2), 0, 0)) {
                            char * verify_text = BN_bn2hex(rnd_vfy);
                            if (NULL != verify_text) {
                                verify_data += verify_text;
                                OPENSSL_free(verify_text);
                            }
                        } else {
                            ATFRAME_GATEWAY_ON_ERROR(static_cast<int>(ERR_get_error()), "openssl/libressl generate verify text failed");
                        }
                        BN_free(rnd_vfy);
                    }

#elif defined(LIBATFRAME_ATGATEWAY_ENABLE_MBEDTLS)
                    {
                        size_t secret_len = outsz >> 1;
                        verify_data.resize(outsz + outsz);
                        // 3 * secret_len, 1 for binary data, 2 for hex data
                        unsigned char* verify_text = (unsigned char*)malloc(secret_len);
                        if (NULL != verify_text) {
                            int res = mbedtls_ctr_drbg_random(&crypt_handshake_->shared_conf->mbedtls_ctr_drbg_,
                                verify_text, secret_len);
                            if (0 == res) {
                                util::string::dumphex(verify_text, secret_len, &verify_data[outsz]);
                            } else {
                                ATFRAME_GATEWAY_ON_ERROR(res, "mbedtls generate verify text failed");
                            }
                            free(verify_text);
                        }
                    }
#endif
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
            // if switch type is direct, read handle should be set here
            if (::atframe::gw::inner::v1::switch_secret_t_EN_SST_DIRECT == body_handshake.switch_type()) {
                crypt_read_ = crypt_handshake_;
            }

            // check crypt info
            int ret = 0;
            if (handshake_.switch_secret_type != body_handshake.switch_type() || !crypt_read_ ||
                crypt_read_->type != body_handshake.crypt_type() ||
                crypt_read_->keybits != body_handshake.crypt_bits()) {
                ATFRAME_GATEWAY_ON_ERROR(error_code_t::EN_ECT_HANDSHAKE, "crypt information between client and server not matched.");
                close(error_code_t::EN_ECT_HANDSHAKE, true);
                return error_code_t::EN_ECT_HANDSHAKE;
            }

            // check hello message prefix
            if (NULL != body_handshake.crypt_param() && !crypt_read_->param.empty() && crypt_read_->param.size() <= body_handshake.crypt_param()->size()) {
                const void* outbuf = NULL;
                size_t outsz = 0;
                ret = decrypt_data(*crypt_read_, body_handshake.crypt_param()->data(), body_handshake.crypt_param()->size(),
                    outbuf, outsz);
                if (0 != ret) {
                    ATFRAME_GATEWAY_ON_ERROR(ret, "verify crypt information but decode failed.");
                } else {
                    const unsigned char *checked_ch = reinterpret_cast<const unsigned char *>(outbuf);
                    for (size_t i = 0; checked_ch && *checked_ch && i < crypt_read_->param.size(); ++i, ++checked_ch) {
                        if (*checked_ch != crypt_read_->param[i]) {
                            ret = error_code_t::EN_ECT_HANDSHAKE;
                            break;
                        }
                    }
                }
            }

            if (0 == ret) {
                // than read key updated
                close_handshake(0);
            } else {
                ATFRAME_GATEWAY_ON_ERROR(error_code_t::EN_ECT_CRYPT_VERIFY, "verify failed.");
                close_handshake(error_code_t::EN_ECT_CRYPT_VERIFY);
                close(close_reason_t::EN_CRT_HANDSHAKE, true);
            }
            return ret;
        }

        int libatgw_proto_inner_v1::pack_handshake_start_rsp(flatbuffers::FlatBufferBuilder &builder, uint64_t sess_id,
            flatbuffers::Offset<::atframe::gw::inner::v1::cs_body_handshake> &handshake_data) {
            using namespace ::atframe::gw::inner::v1;

            int ret = 0;
            // if not use crypt, assign crypt information and close_handshake(0)
            if (0 == sess_id || !crypt_handshake_->shared_conf ||
                ::atframe::gw::inner::v1::crypt_type_t_EN_ET_NONE == crypt_handshake_->shared_conf->conf_.type) {
                // empty data
                handshake_data = Createcs_body_handshake(builder, sess_id, handshake_step_t_EN_HST_START_RSP,
                    switch_secret_t_EN_SST_DIRECT, crypt_type_t_EN_ET_NONE, 0,
                    builder.CreateVector(reinterpret_cast<const int8_t *>(NULL), 0)
                );

                crypt_handshake_->setup(::atframe::gw::inner::v1::crypt_type_t_EN_ET_NONE, 0);
                crypt_read_ = crypt_handshake_;
                crypt_write_ = crypt_handshake_;
                close_handshake(0);
                return ret;
            }

            crypt_handshake_->param.clear();
            switch (handshake_.switch_secret_type) {
            case ::atframe::gw::inner::v1::switch_secret_t_EN_SST_DIRECT: {
                // generate a secret key
#if defined(LIBATFRAME_ATGATEWAY_ENABLE_OPENSSL) || defined(LIBATFRAME_ATGATEWAY_ENABLE_LIBRESSL)
                BIGNUM *rnd_sec = BN_new();
                if (NULL != rnd_sec) {
                    if (1 == BN_rand(rnd_sec, static_cast<int>(crypt_handshake_->shared_conf->conf_.keybits), 0, 0)) {
                        crypt_handshake_->secret.resize(BN_num_bytes(rnd_sec));
                        BN_bn2bin(rnd_sec, &crypt_handshake_->secret[0]);
                    } else {
                        ATFRAME_GATEWAY_ON_ERROR(static_cast<int>(ERR_get_error()), "openssl/libressl generate bignumber failed");
                    }
                    BN_free(rnd_sec);
                }
#elif defined(LIBATFRAME_ATGATEWAY_ENABLE_MBEDTLS)
                {
                    size_t secret_len = crypt_handshake_->shared_conf->conf_.keybits / 8;
                    crypt_handshake_->secret.resize(secret_len);
                    int res = mbedtls_ctr_drbg_random(&crypt_handshake_->shared_conf->mbedtls_ctr_drbg_, 
                        &crypt_handshake_->secret[0], secret_len);
                    if (0 != res) {
                        ATFRAME_GATEWAY_ON_ERROR(res, "mbedtls generate bignumber failed");
                    }
                }
#endif
                if (crypt_handshake_->secret.empty()) {
                    crypt_handshake_->secret.resize(crypt_handshake_->shared_conf->conf_.default_key.size());
                    memcpy(crypt_handshake_->secret.data(), crypt_handshake_->shared_conf->conf_.default_key.data(), crypt_handshake_->shared_conf->conf_.default_key.size());
                }

                crypt_handshake_->setup(crypt_handshake_->shared_conf->conf_.type, crypt_handshake_->shared_conf->conf_.keybits);
                crypt_write_ = crypt_handshake_;

                handshake_data = Createcs_body_handshake(builder, sess_id, handshake_step_t_EN_HST_START_RSP,
                    static_cast< ::atframe::gw::inner::v1::switch_secret_t>(handshake_.switch_secret_type), 
                    static_cast< ::atframe::gw::inner::v1::crypt_type_t>(crypt_handshake_->type), 
                    crypt_handshake_->keybits,
                    builder.CreateVector(reinterpret_cast<const int8_t *>(crypt_handshake_->secret.data()), crypt_handshake_->secret.size())
                );

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
                    // dump P,G,GX
                    // @see int ssl3_send_server_key_exchange(SSL *s) in s3_srvr.c
                    {
                        BIGNUM *r[4] = {handshake_.dh.openssl_dh_ptr_->p, handshake_.dh.openssl_dh_ptr_->g,
                                        handshake_.dh.openssl_dh_ptr_->pub_key, NULL};

                        size_t olen = 0;
                        unsigned int nr[4] = {0};
                        for (int i = 0; i < 4 && r[i] != NULL; i++) {
                            nr[i] = BN_num_bytes(r[i]);
                            // DHM_MPI_EXPORT in mbedtls/polarssl use 2 byte to store length, so openssl/libressl should use OPENSSL_NO_SRP
                            olen += static_cast<size_t>(nr[i] + 2);
                        }

                        crypt_handshake_->param.resize(olen, 0);
                        unsigned char *p = &crypt_handshake_->param[0];
                        for (int i = 0; i < 4 && r[i] != NULL; i++) {
                            s2n(nr[i], p);
                            BN_bn2bin(r[i], p);
                            p += nr[i];
                        }
                    }

#elif defined(LIBATFRAME_ATGATEWAY_ENABLE_MBEDTLS)
                    if (false == handshake_.has_data) {
                        ret = error_code_t::EN_ECT_CRYPT_NOT_SUPPORTED;
                        ATFRAME_GATEWAY_ON_ERROR(ret, "mbedtls DH not setup");
                        break;
                    }

                    // size is P,G,GX
                    size_t psz = mbedtls_mpi_size(&handshake_.dh.mbedtls_dh_ctx_.P);
                    size_t gsz = mbedtls_mpi_size(&handshake_.dh.mbedtls_dh_ctx_.G);
                    size_t olen = 0;
                    // @see mbedtls_dhm_make_params, output P,G,GX. GX is smaller than P
                    crypt_handshake_->param.resize(psz + psz + gsz, 0);
                    int res = mbedtls_dhm_make_params(&handshake_.dh.mbedtls_dh_ctx_, static_cast<int>(psz),
                                                      reinterpret_cast<unsigned char *>(&crypt_handshake_->param[0]), &olen,
                                                      mbedtls_ctr_drbg_random,
                                                      &crypt_handshake_->shared_conf->mbedtls_ctr_drbg_);
                    if (0 != res) {
                        ATFRAME_GATEWAY_ON_ERROR(res, "mbedtls DH generate check public key failed");
                        ret = error_code_t::EN_ECT_CRYPT_NOT_SUPPORTED;
                        break;
                    }

                    // resize if P,G,GX is small than crypt_handshake_->param
                    assert(olen <= psz);
                    if (olen < crypt_handshake_->param.size()) {
                        crypt_handshake_->param.resize(olen);
                    }
#endif

                } while (false);
                // send send first parameter
                handshake_data = Createcs_body_handshake(builder, sess_id, handshake_step_t_EN_HST_START_RSP,
                    static_cast< ::atframe::gw::inner::v1::switch_secret_t>(handshake_.switch_secret_type),
                    static_cast< ::atframe::gw::inner::v1::crypt_type_t>(crypt_handshake_->shared_conf->conf_.type),
                    crypt_handshake_->shared_conf->conf_.keybits,
                    builder.CreateVector(reinterpret_cast<const int8_t *>(crypt_handshake_->param.data()), crypt_handshake_->param.size())
                );

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
            flatbuffers::Offset<::atframe::gw::inner::v1::cs_body_handshake> &handshake_data) {
            using namespace ::atframe::gw::inner::v1;

            int ret = 0;
            if (0 == peer_body.session_id() || NULL == peer_body.crypt_param() || !crypt_handshake_->shared_conf) {
                // empty data
                handshake_data = Createcs_body_handshake(builder, peer_body.session_id(), handshake_step_t_EN_HST_DH_PUBKEY_REQ,
                    switch_secret_t_EN_SST_DIRECT, crypt_type_t_EN_ET_NONE, 0,
                    builder.CreateVector(reinterpret_cast<const int8_t *>(NULL), 0)
                );

                return error_code_t::EN_ECT_SESSION_NOT_FOUND;
            }

            handshake_.switch_secret_type = peer_body.switch_type();
            crypt_handshake_->param.clear();

#if defined(LIBATFRAME_ATGATEWAY_ENABLE_OPENSSL) || defined(LIBATFRAME_ATGATEWAY_ENABLE_LIBRESSL)
            BIGNUM *pubkey = NULL;
#endif
            do {
#if defined(LIBATFRAME_ATGATEWAY_ENABLE_OPENSSL) || defined(LIBATFRAME_ATGATEWAY_ENABLE_LIBRESSL)
                if (NULL == handshake_.dh.openssl_dh_ptr_ || false == handshake_.has_data) {
                    ret = error_code_t::EN_ECT_CRYPT_NOT_SUPPORTED;
                    ATFRAME_GATEWAY_ON_ERROR(ret, "openssl/libressl DH not setup");
                    break;
                }

                // ===============================================
                // import P,G,GY
                // @see int ssl3_get_key_exchange(SSL *s) in s3_clnt.c or s3_clnt.c
                {
                    unsigned int i = 0, param_len = 2, n = static_cast<unsigned int>(peer_body.crypt_param()->size());
                    const unsigned char *p = reinterpret_cast<const unsigned char *>(peer_body.crypt_param()->data());

                    // P
                    if (param_len > n) {
                        ATFRAME_GATEWAY_ON_ERROR(ret, "openssl/libressl read DH parameter: P SSL_R_LENGTH_TOO_SHORT");
                        ret = error_code_t::EN_ECT_CRYPT_INIT_DHPARAM;
                        break;
                    }
                    n2s(p, i);

                    if (i > n - param_len) {
                        ATFRAME_GATEWAY_ON_ERROR(ret, "openssl/libressl read DH parameter: P SSL_R_BAD_DH_P_LENGTH");
                        ret = error_code_t::EN_ECT_CRYPT_INIT_DHPARAM;
                        break;
                    }

                    param_len += i;
                    if (!(handshake_.dh.openssl_dh_ptr_->p = BN_bin2bn(p, i, NULL))) {
                        ATFRAME_GATEWAY_ON_ERROR(ret, "openssl/libressl read DH parameter: P ERR_R_BN_LIB");
                        ret = error_code_t::EN_ECT_CRYPT_INIT_DHPARAM;
                        break;
                    }
                    p += i;

                    if (BN_is_zero(handshake_.dh.openssl_dh_ptr_->p)) {
                        ATFRAME_GATEWAY_ON_ERROR(ret, "openssl/libressl read DH parameter: P SSL_R_BAD_DH_P_VALUE");
                        ret = error_code_t::EN_ECT_CRYPT_INIT_DHPARAM;
                        break;
                    }

                    // G
                    param_len += 2;
                    if (param_len > n) {
                        ATFRAME_GATEWAY_ON_ERROR(ret, "openssl/libressl read DH parameter: G SSL_R_LENGTH_TOO_SHORT");
                        ret = error_code_t::EN_ECT_CRYPT_INIT_DHPARAM;
                        break;
                    }
                    n2s(p, i);

                    if (i > n - param_len) {
                        ATFRAME_GATEWAY_ON_ERROR(ret, "openssl/libressl read DH parameter: G SSL_R_BAD_DH_G_LENGTH");
                        ret = error_code_t::EN_ECT_CRYPT_INIT_DHPARAM;
                        break;
                    }

                    param_len += i;
                    if (!(handshake_.dh.openssl_dh_ptr_->g = BN_bin2bn(p, i, NULL))) {
                        ATFRAME_GATEWAY_ON_ERROR(ret, "openssl/libressl read DH parameter: G ERR_R_BN_LIB");
                        ret = error_code_t::EN_ECT_CRYPT_INIT_DHPARAM;
                        break;
                    }
                    p += i;

                    if (BN_is_zero(handshake_.dh.openssl_dh_ptr_->g)) {
                        ATFRAME_GATEWAY_ON_ERROR(ret, "openssl/libressl read DH parameter: G SSL_R_BAD_DH_G_VALUE");
                        ret = error_code_t::EN_ECT_CRYPT_INIT_DHPARAM;
                        break;
                    }

                    // GY
                    param_len += 2;
                    if (param_len > n) {
                        ATFRAME_GATEWAY_ON_ERROR(ret, "openssl/libressl read DH parameter: GY SSL_R_LENGTH_TOO_SHORT");
                        ret = error_code_t::EN_ECT_CRYPT_INIT_DHPARAM;
                        break;
                    }
                    n2s(p, i);

                    if (i > n - param_len) {
                        ATFRAME_GATEWAY_ON_ERROR(ret, "openssl/libressl read DH parameter: GY SSL_R_BAD_DH_PUB_KEY_LENGTH");
                        ret = error_code_t::EN_ECT_CRYPT_INIT_DHPARAM;
                        break;
                    }

                    param_len += i;
                    if (!(pubkey = BN_bin2bn(p, i, NULL))) {
                        ATFRAME_GATEWAY_ON_ERROR(ret, "openssl/libressl read DH parameter: GY ERR_R_BN_LIB");
                        ret = error_code_t::EN_ECT_CRYPT_INIT_DHPARAM;
                        break;
                    }
                    p += i;

                    if (BN_is_zero(pubkey)) {
                        ATFRAME_GATEWAY_ON_ERROR(ret, "openssl/libressl read DH parameter: GY SSL_R_BAD_DH_PUB_KEY_VALUE");
                        ret = error_code_t::EN_ECT_CRYPT_INIT_DHPARAM;
                        break;
                    }
                }

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
                crypt_handshake_->secret.resize(static_cast<size_t>(sizeof(unsigned char) * (DH_size(handshake_.dh.openssl_dh_ptr_))), 0);
                errcode =
                    DH_compute_key(reinterpret_cast<unsigned char *>(&crypt_handshake_->secret[0]), pubkey, handshake_.dh.openssl_dh_ptr_);
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
                                              reinterpret_cast<unsigned char *>(&crypt_handshake_->param[0]), psz,
                                              mbedtls_ctr_drbg_random,
                                              &crypt_handshake_->shared_conf->mbedtls_ctr_drbg_);
                if (0 != res) {
                    ATFRAME_GATEWAY_ON_ERROR(res, "mbedtls DH make public key failed");
                    ret = error_code_t::EN_ECT_CRYPT_NOT_SUPPORTED;
                    break;
                }

                // generate secret
                crypt_handshake_->secret.resize(psz, 0);
                res =
                    mbedtls_dhm_calc_secret(&handshake_.dh.mbedtls_dh_ctx_, reinterpret_cast<unsigned char *>(&crypt_handshake_->secret[0]),
                                            psz, &psz, mbedtls_ctr_drbg_random,
                                            &crypt_handshake_->shared_conf->mbedtls_ctr_drbg_);
                if (0 != res) {
                    ATFRAME_GATEWAY_ON_ERROR(res, "mbedtls DH compute key failed");
                    ret = error_code_t::EN_ECT_CRYPT_NOT_SUPPORTED;
                    break;
                }

                crypt_handshake_->setup(peer_body.crypt_type(), peer_body.crypt_bits());
                crypt_write_ = crypt_handshake_;
#endif
            } while (false);

#if defined(LIBATFRAME_ATGATEWAY_ENABLE_OPENSSL) || defined(LIBATFRAME_ATGATEWAY_ENABLE_LIBRESSL)
            if (NULL != pubkey) {
                BN_free(pubkey);
                pubkey = NULL;
            }
#endif
            // send send first parameter
            handshake_data = Createcs_body_handshake(builder, peer_body.session_id(), handshake_step_t_EN_HST_DH_PUBKEY_REQ,
                static_cast< ::atframe::gw::inner::v1::switch_secret_t>(handshake_.switch_secret_type), 
                peer_body.crypt_type(), peer_body.crypt_bits(),
                builder.CreateVector(reinterpret_cast<const int8_t *>(crypt_handshake_->param.data()), crypt_handshake_->param.size())
            );

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
                    // client mode, just init , do not read PEM file
                    if (crypt_handshake_->shared_conf->conf_.client_mode) {
                        handshake_.dh.openssl_dh_ptr_ = DH_new();
                    } else {
                        UNUSED(BIO_reset(crypt_handshake_->shared_conf->openssl_dh_bio_));
                        handshake_.dh.openssl_dh_ptr_ =
                            PEM_read_bio_DHparams(crypt_handshake_->shared_conf->openssl_dh_bio_, NULL, NULL, NULL);
                    }
                    if (!handshake_.dh.openssl_dh_ptr_) {
                        ret = error_code_t::EN_ECT_CRYPT_INIT_DHPARAM;
                        ATFRAME_GATEWAY_ON_ERROR(ret, "openssl/libressl parse dhm failed");
                        break;
                    }

                    if (!crypt_handshake_->shared_conf->conf_.client_mode && 1 != DH_generate_key(handshake_.dh.openssl_dh_ptr_)) {
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

                    // client mode, just init , do not read PEM file
                    if (false == crypt_handshake_->shared_conf->conf_.client_mode) {
                        int res = mbedtls_dhm_parse_dhm(
                            &handshake_.dh.mbedtls_dh_ctx_,
                            reinterpret_cast<const unsigned char *>(crypt_handshake_->shared_conf->mbedtls_dh_param_.data()),
                            crypt_handshake_->shared_conf->mbedtls_dh_param_.size());
                        if (0 != res) {
                            ATFRAME_GATEWAY_ON_ERROR(res, "mbedtls parse dhm failed");
                            ret = error_code_t::EN_ECT_CRYPT_INIT_DHPARAM;
                            break;
                        }
                    }
                } while (false);

                if (0 != ret) {
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
            if (handshake_.has_data) {
                // ready to update handshake
                set_flag(flag_t::EN_PFT_HANDSHAKE_UPDATE, true);
            }
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

                if (NULL != callbacks_ && callbacks_->close_fn) {
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
            close_reason_ = reason;

            // send kickoff message
            if (is_send_kickoff) {
                send_kickoff(reason);
            }

            // must set flag after send_kickoff(reason), because it will still use resources
            set_flag(flag_t::EN_PFT_CLOSING, true);

            // wait writing to finished
            if (!check_flag(flag_t::EN_PFT_WRITING)) {
                set_flag(flag_t::EN_PFT_CLOSED, true);

                if (NULL != callbacks_ && callbacks_->close_fn) {
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

            // use read handle first, maybe the new handshake not finished
            crypt_session_ptr_t other_crypt_handshake = other_proto->crypt_read_;
            if (!other_crypt_handshake) {
                other_crypt_handshake = other_proto->crypt_handshake_;
            }

            do {
                std::vector<unsigned char> handshake_secret;
                int crypt_type = 0;
                uint32_t crypt_keybits = 128;
                if (NULL == handshake_.ext_data) {
                    ret = false;
                    break;
                } else {
                    const ::atframe::gw::inner::v1::cs_body_handshake* body_handshake = reinterpret_cast<const ::atframe::gw::inner::v1::cs_body_handshake*>(handshake_.ext_data);
                    const flatbuffers::Vector<int8_t> *secret = body_handshake->crypt_param();
                    if (NULL != secret) {
                        handshake_secret.resize(secret->size());
                        memcpy(handshake_secret.data(), secret->data(), secret->size());
                    }

                    crypt_type = body_handshake->crypt_type();
                    crypt_keybits = body_handshake->crypt_bits();
                }


                // check crypt type and keybits
                if (crypt_type != other_crypt_handshake->type || crypt_keybits != other_crypt_handshake->keybits) {
                    ret = false;
                    break;
                }

                if (::atframe::gw::inner::v1::crypt_type_t_EN_ET_NONE == other_crypt_handshake->type) {
                    ret = true;
                    break;
                }

                // decrypt secret
                const void *outbuf = NULL;
                size_t outsz = 0;
                if (0 !=
                    decrypt_data(*other_crypt_handshake, handshake_secret.data(),
                        handshake_secret.size(), outbuf, outsz)) {
                    ret = false;
                    break;
                }

                // compare secret and encrypted secret
                if (NULL == outbuf || outsz != other_crypt_handshake->secret.size() ||
                    0 != memcmp(outbuf, other_crypt_handshake->secret.data(), outsz)) {
                    ret = false;
                }
            } while (false);

            // if success, copy crypt information
            if (ret) {
                session_id_ = other_proto->session_id_;
                crypt_handshake_ = other_crypt_handshake;
                // setup handshake
                setup_handshake(other_crypt_handshake->shared_conf);
                crypt_read_ = crypt_handshake_;
                crypt_write_ = crypt_handshake_;
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

        std::string libatgw_proto_inner_v1::get_info() const {
            std::stringstream ss;
            size_t limit_sz = 0;
            ss << "atgateway inner protocol: session id=" << session_id_ << std::endl;
            ss << "    last ping delta=" << ping_.last_delta << std::endl;
            ss << "    handshake=" << (handshake_.has_data? "running": "not running")<< ", switch type="<< handshake_.switch_secret_type << std::endl;
            ss << "    status: writing="<< check_flag(flag_t::EN_PFT_WRITING)<<
                ",closing="<< check_flag(flag_t::EN_PFT_CLOSING) <<
                ",closed="<< check_flag(flag_t::EN_PFT_CLOSED) <<
                ",handshake done="<< check_flag(flag_t::EN_PFT_HANDSHAKE_DONE) <<
                ",handshake update="<< check_flag(flag_t::EN_PFT_HANDSHAKE_UPDATE) << std::endl;

            if (read_buffers_.limit().limit_size_ > 0) {
                limit_sz = read_buffers_.limit().limit_size_ + sizeof(read_head_.buffer) - read_head_.len - read_buffers_.limit().cost_size_;
                ss << "    read buffer: used size=" << (read_head_.len + read_buffers_.limit().cost_size_) << ", free size=" << limit_sz << std::endl;
            } else {
                ss << "    read buffer: used size=" << (read_head_.len + read_buffers_.limit().cost_size_) << ", free size=unlimited" << std::endl;
            }

            if (write_buffers_.limit().limit_size_ > 0) {
                limit_sz = write_buffers_.limit().limit_size_ - write_buffers_.limit().cost_size_;
                ss << "    write buffer: used size=" << write_buffers_.limit().cost_size_ << ", free size=" << limit_sz << std::endl;
            } else {
                ss << "    write buffer: used size=" << write_buffers_.limit().cost_size_ << ", free size=unlimited" << std::endl;
            }

#define DUMP_INFO(name, h) \
            if (h) { \
                if (&h != &crypt_handshake_ && h == crypt_handshake_) {\
                    ss << "    "<< name<< " handle: == handshake handle" << std::endl; \
                } else {\
                    ss << "    " << name << " handle: crypt type=" << h->type << ", crypt keybits=" << h->keybits << ", crypt secret="; \
                    util::string::dumphex(h->secret.data(), h->secret.size(), ss); \
                    ss << std::endl; \
                }\
            } else { \
                ss << "    "<< name<< " handle: unset" << std::endl; \
            }

            DUMP_INFO("read", crypt_read_);
            DUMP_INFO("write", crypt_write_);
            DUMP_INFO("handshake", crypt_handshake_);

#undef DUMP_INFO

            return ss.str();
        }

        int libatgw_proto_inner_v1::start_session() {
            if (check_flag(flag_t::EN_PFT_CLOSING)) {
                return error_code_t::EN_ECT_CLOSING;
            }

            if (0 != session_id_) {
                return error_code_t::EN_ECT_SESSION_ALREADY_EXIST;
            }

            if (NULL == callbacks_ || !callbacks_->write_fn) {
                return error_code_t::EN_ECT_MISS_CALLBACKS;
            }

            using namespace ::atframe::gw::inner::v1;

            flatbuffers::FlatBufferBuilder builder;
            flatbuffers::Offset<cs_msg_head> header_data = Createcs_msg_head(builder, cs_msg_type_t_EN_MTT_HANDSHAKE,
                ::atframe::gateway::detail::alloc_seq());
            flatbuffers::Offset<cs_body_handshake> handshake_body;

            handshake_body = Createcs_body_handshake(builder, 0, handshake_step_t_EN_HST_START_REQ,
                switch_secret_t_EN_SST_DIRECT, crypt_type_t_EN_ET_NONE, 0, 
                builder.CreateVector(reinterpret_cast<const int8_t *>(NULL), 0)
            );

            builder.Finish(Createcs_msg(builder, header_data, cs_msg_body_cs_body_handshake, handshake_body.Union()), cs_msgIdentifier());
            return write_msg(builder);
        }

        int libatgw_proto_inner_v1::reconnect_session(uint64_t sess_id, int type, const std::vector<unsigned char> &secret, uint32_t keybits) {
            if (check_flag(flag_t::EN_PFT_CLOSING)) {
                return error_code_t::EN_ECT_CLOSING;
            }

            if (NULL == callbacks_ || !callbacks_->write_fn) {
                return error_code_t::EN_ECT_MISS_CALLBACKS;
            }

            // encrypt secrets
            crypt_handshake_->secret = secret;
            crypt_handshake_->setup(type, keybits);

            const void *secret_buffer = NULL;
            size_t secret_length = secret.size();
            encrypt_data(*crypt_handshake_, secret.data(), secret.size(), secret_buffer, secret_length);

            using namespace ::atframe::gw::inner::v1;

            flatbuffers::FlatBufferBuilder builder;
            flatbuffers::Offset<cs_msg_head> header_data = Createcs_msg_head(builder, cs_msg_type_t_EN_MTT_HANDSHAKE,
                ::atframe::gateway::detail::alloc_seq());
            flatbuffers::Offset<cs_body_handshake> handshake_body;

            handshake_body = Createcs_body_handshake(builder, sess_id, handshake_step_t_EN_HST_RECONNECT_REQ,
                static_cast<switch_secret_t>(handshake_.switch_secret_type), 
                static_cast< ::atframe::gw::inner::v1::crypt_type_t>(type), keybits,
                builder.CreateVector(reinterpret_cast<const int8_t *>(secret_buffer), secret_length)
            );

            builder.Finish(Createcs_msg(builder, header_data, cs_msg_body_cs_body_handshake, handshake_body.Union()), cs_msgIdentifier());
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
            using namespace ::atframe::gw::inner::v1;

            flatbuffers::FlatBufferBuilder builder;
            flatbuffers::Offset<cs_msg_head> header_data = Createcs_msg_head(builder, msg_type,
                ::atframe::gateway::detail::alloc_seq());

            flatbuffers::Offset<cs_body_post> post_body = Createcs_body_post(builder, static_cast<uint64_t>(ori_len),
                builder.CreateVector(reinterpret_cast<const int8_t *>(buffer), len)
            );

            builder.Finish(Createcs_msg(builder, header_data, cs_msg_body_cs_body_post, post_body.Union()), cs_msgIdentifier());
            return write_msg(builder);
        }

        int libatgw_proto_inner_v1::send_post(const void *buffer, size_t len) {
            return send_post(::atframe::gw::inner::v1::cs_msg_type_t_EN_MTT_POST, buffer, len);
        }

        int libatgw_proto_inner_v1::send_ping() {
            if (check_flag(flag_t::EN_PFT_CLOSING)) {
                return error_code_t::EN_ECT_CLOSING;
            }

            if (NULL == callbacks_ || !callbacks_->write_fn) {
                return error_code_t::EN_ECT_MISS_CALLBACKS;
            }

            ping_.last_ping = ping_data_t::clk_t::now();

            using namespace ::atframe::gw::inner::v1;

            flatbuffers::FlatBufferBuilder builder;
            flatbuffers::Offset<cs_msg_head> header_data = Createcs_msg_head(builder, cs_msg_type_t_EN_MTT_PING,
                ::atframe::gateway::detail::alloc_seq());

            flatbuffers::Offset<cs_body_ping> ping_body = Createcs_body_ping(builder, 
                static_cast<int64_t>(ping_.last_ping.time_since_epoch().count())
            );

            builder.Finish(Createcs_msg(builder, header_data, cs_msg_body_cs_body_ping, ping_body.Union()), cs_msgIdentifier());
            return write_msg(builder);
        }

        int libatgw_proto_inner_v1::send_pong(int64_t tp) {
            if (check_flag(flag_t::EN_PFT_CLOSING)) {
                return error_code_t::EN_ECT_CLOSING;
            }

            if (NULL == callbacks_ || !callbacks_->write_fn) {
                return error_code_t::EN_ECT_MISS_CALLBACKS;
            }

            using namespace ::atframe::gw::inner::v1;

            flatbuffers::FlatBufferBuilder builder;
            flatbuffers::Offset<cs_msg_head> header_data = Createcs_msg_head(builder, cs_msg_type_t_EN_MTT_PONG,
                ::atframe::gateway::detail::alloc_seq());

            flatbuffers::Offset<cs_body_ping> ping_body = Createcs_body_ping(builder, tp);

            builder.Finish(Createcs_msg(builder, header_data, cs_msg_body_cs_body_ping, ping_body.Union()), cs_msgIdentifier());
            return write_msg(builder);
        }

        int libatgw_proto_inner_v1::send_key_syn() {
            if (check_flag(flag_t::EN_PFT_CLOSING)) {
                return error_code_t::EN_ECT_CLOSING;
            }

            if (::atframe::gw::inner::v1::crypt_type_t_EN_ET_NONE == crypt_handshake_->type) {
                return 0;
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

            if (!global_cfg || global_cfg->conf_.client_mode) {
                return ret;
            }

            using namespace ::atframe::gw::inner::v1;

            flatbuffers::FlatBufferBuilder builder;
            flatbuffers::Offset<cs_msg_head> header_data = Createcs_msg_head(builder, cs_msg_type_t_EN_MTT_POST_KEY_SYN,
                ::atframe::gateway::detail::alloc_seq());

            flatbuffers::Offset<cs_body_handshake> handshake_body;
            ret = pack_handshake_start_rsp(builder, session_id_, handshake_body);
            if (ret < 0) {
                handshake_done(ret);
                return ret;
            }

            builder.Finish(Createcs_msg(builder, header_data, cs_msg_body_cs_body_handshake, handshake_body.Union()), cs_msgIdentifier());
            ret = write_msg(builder);
            if (ret < 0) {
                handshake_done(ret);
            }
            return ret;
        }

        int libatgw_proto_inner_v1::send_kickoff(int reason) {
            if (check_flag(flag_t::EN_PFT_CLOSING)) {
                return error_code_t::EN_ECT_CLOSING;
            }

            if (NULL == callbacks_ || !callbacks_->write_fn) {
                return error_code_t::EN_ECT_MISS_CALLBACKS;
            }

            using namespace ::atframe::gw::inner::v1;

            flatbuffers::FlatBufferBuilder builder;
            flatbuffers::Offset<cs_msg_head> header_data = Createcs_msg_head(builder, cs_msg_type_t_EN_MTT_KICKOFF,
                ::atframe::gateway::detail::alloc_seq());

            flatbuffers::Offset<cs_body_kickoff> kickoff_body = Createcs_body_kickoff(builder, reason);

            builder.Finish(Createcs_msg(builder, header_data, cs_msg_body_cs_body_kickoff, kickoff_body.Union()), cs_msgIdentifier());
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
            uint64_t sess_id = session_id_;

            const void *outbuf = NULL;
            size_t outsz = 0;
            int ret = 0;
            if (NULL != buf && sz > 0) {
                ret = encrypt_data(*crypt_write_, buf, sz, outbuf, outsz);
            }

            if (0 != ret) {
                sess_id = 0;
                outbuf = NULL;
                outsz = 0;
            }

            using namespace ::atframe::gw::inner::v1;

            flatbuffers::FlatBufferBuilder builder;
            flatbuffers::Offset<cs_msg_head> header_data = Createcs_msg_head(builder, cs_msg_type_t_EN_MTT_HANDSHAKE,
                ::atframe::gateway::detail::alloc_seq());

            flatbuffers::Offset<cs_body_handshake> verify_body = Createcs_body_handshake(builder, sess_id, handshake_step_t_EN_HST_VERIFY,
                static_cast< ::atframe::gw::inner::v1::switch_secret_t>(handshake_.switch_secret_type),
                static_cast< ::atframe::gw::inner::v1::crypt_type_t>(crypt_write_->type),
                crypt_write_->keybits,
                builder.CreateVector<int8_t>(reinterpret_cast<const int8_t*>(outbuf), outsz)
            );

            builder.Finish(Createcs_msg(builder, header_data, cs_msg_body_cs_body_handshake, verify_body.Union()), cs_msgIdentifier());
            return write_msg(builder);
        }

        const libatgw_proto_inner_v1::crypt_session_ptr_t& libatgw_proto_inner_v1::get_crypt_read() const {
            return crypt_read_;
        }

        const libatgw_proto_inner_v1::crypt_session_ptr_t& libatgw_proto_inner_v1::get_crypt_write() const {
            return crypt_write_;
        }

        const libatgw_proto_inner_v1::crypt_session_ptr_t& libatgw_proto_inner_v1::get_crypt_handshake() const {
            return crypt_handshake_;
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
                outsz = ((insz - 1) | 0x03) + 1;

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

                outsz = ((insz - 1) | (AES_BLOCK_SIZE - 1)) + 1;

                if (len < outsz) {
                    return error_code_t::EN_ECT_MSG_TOO_LARGE;
                }

                memcpy(buffer, in, insz);
                if (outsz > insz) {
                    memset(reinterpret_cast<char *>(buffer) + insz, 0, outsz - insz);
                }

#if defined(LIBATFRAME_ATGATEWAY_ENABLE_OPENSSL) || defined(LIBATFRAME_ATGATEWAY_ENABLE_LIBRESSL)
                AES_cbc_encrypt(reinterpret_cast<const unsigned char *>(buffer), reinterpret_cast<unsigned char *>(buffer), outsz,
                                &crypt_info.aes_key.openssl_encrypt_key, iv, AES_ENCRYPT);

#elif defined(LIBATFRAME_ATGATEWAY_ENABLE_MBEDTLS)
                ret = mbedtls_aes_crypt_cbc(&crypt_info.aes_key.mbedtls_aes_encrypt_ctx, MBEDTLS_AES_ENCRYPT, outsz, iv,
                                            reinterpret_cast<const unsigned char *>(buffer), reinterpret_cast<unsigned char *>(buffer));
                if (ret < 0) {
                    ATFRAME_GATEWAY_ON_ERROR(ret, "mbedtls AES encrypt failed");
                    return ret;
                }
#endif
                out = buffer;
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
                outsz = ((insz - 1) | 0x03) + 1;

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

                outsz = ((insz - 1) | (AES_BLOCK_SIZE - 1)) + 1;

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
                out = buffer;
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
