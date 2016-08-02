#include "libatgw_proto_inner_v1.h"

namespace atframe {
    namespace gateway {
        libatgw_proto_inner_v1::libatgw_proto_inner_v1() {
            crypt_info_.type = ::atframe::gw::inner::v1::crypt_type_t_EN_ET_NONE;
            crypt_info_.keybits = 0;

            read_head_.len = 0;
        }


        libatgw_proto_inner_v1::~libatgw_proto_inner_v1() {}

        void libatgw_proto_inner_v1::alloc_recv_buffer(size_t suggested_size, char *&out_buf, size_t &out_len) {
            flag_guard_t flag_guard(flags_, flag_t::EN_FT_IN_CALLBACK);

            // 如果正处于关闭阶段，忽略所有数据
            if (check_flag(flag_t::EN_FT_CLOSING)) {
                out_buf = NULL;
                out_len = 0;
                return;
            }

            void *data = NULL;
            size_t sread = 0, swrite = 0;
            read_buffers_.back(data, sread, swrite);

            // reading length and hash code, use small buffer block
            if (NULL == data || 0 == swrite) {
                buf->len = sizeof(read_head_.buffer) - read_head_.len;

                if (0 == buf->len) {
                    // hash code and length shouldn't be greater than small buffer block
                    buf->base = NULL;
                    assert(false);
                } else {
                    buf->base = &read_head_.buffer[read_head_.len];
                }
                return;
            }

            // 否则指定为大内存块缓冲区
            buf->base = reinterpret_cast<char *>(data);
            buf->len = swrite;
        }

        void libatgw_proto_inner_v1::read(int ssz, const char *buff, size_t nread_s, int &errcode) {
            errcode = error_code_t::EN_ECT_SUCCESS;
            flag_guard_t flag_guard(flags_, flag_t::EN_FT_IN_CALLBACK);

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

        void libatgw_proto_inner_v1::dispatch_data(const char *buff, size_t len, int status, int errcode) {
            // do nothing if any error
            if (errcode < 0) {
                return;
            }
            // TODO unpack
            // TODO unzip
            // TODO decrypt
            // TODO on_message
        }

        int libatgw_proto_inner_v1::try_write() {
            if (NULL == callbacks_ || !callbacks_->write_fn) {
                return error_code_t::EN_ECT_MISS_CALLBACKS;
            }

            int ret = 0;
            if (check_flag(flag_t::EN_PFT_WRITING)) {
                return ret;
            }

            void *buffer = NULL;
            size_t sz = 0;
            bool is_done = false;
            // TODO merge messages

            set_flag(flag_t::EN_PFT_WRITING, true);
            ret = callbacks_->write_fn(this, buffer, sz, &is_done);
            if (is_done) {
                set_flag(flag_t::EN_PFT_WRITING, false);
            }

            return ret;
        }

        int libatgw_proto_inner_v1::write_msg(::atframe::gw::inner::v1::cs_msg &msg) {
            // first 32bits is hash code, and then 32bits length
            const size_t msg_header_len = sizeof(uint32_t) + sizeof(uint32_t);

            void *buf = NULL;
            size_t len = 0;
            // TODO pack message

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
            if (NULL == callbacks_ || !callbacks_->write_fn) {
                return error_code_t::EN_ECT_MISS_CALLBACKS;
            }

            // TODO encrypt
            // TODO zip
            // TODO push data
            // TODO if not writing, try merge data and write it
            return try_write();
        }

        int libatgw_proto_inner_v1::write_done(int status) {
            if (!check_flag(flag_t::EN_PFT_WRITING)) {
                return 0;
            }
            set_flag(flag_t::EN_PFT_WRITING, false);

            // TODO pop front message queue
            // try write next data
            return try_write();
        }

        int libatgw_proto_inner_v1::close(int reason) {
            // TODO send kickoff message
            if (check_flag(flag_t::EN_FT_CLOSING)) {
                return 0;
            }
            set_flag(flag_t::EN_FT_CLOSING, true);

            if (NULL != callbacks_ || callbacks_->close_fn) {
                return callbacks_->close_fn(this, reason);
            }
            return 0;
        }

        bool libatgw_proto_inner_v1::check_reconnect(proto_base *other) {
            // TODO check crypt type, keybits and encrypted secret
            return true;
        }

        int libatgw_proto_inner_v1::global_reload(crypt_conf_t &crypt_conf) {
            // TODO spin_lock
            if (::atframe::gw::inner::v1::crypt_type_t_EN_ET_NONE == crypt_conf.type) {
                return 0;
            }

            switch (crypt_conf.switch_secret_type) {
            case ::atframe::gw::inner::v1::switch_secret_t_EN_SST_DH: {
// TODO init DH param file
#if defined(LIBATFRAME_ATGATEWAY_ENABLE_OPENSSL)
#elif defined(LIBATFRAME_ATGATEWAY_ENABLE_MBEDTLS)
#elif defined(LIBATFRAME_ATGATEWAY_ENABLE_LIBRESSL)
#endif
                break;
            }
            case ::atframe::gw::inner::v1::switch_secret_t_EN_SST_RSA: {
// TODO init public key and private key
#if defined(LIBATFRAME_ATGATEWAY_ENABLE_OPENSSL)
#elif defined(LIBATFRAME_ATGATEWAY_ENABLE_MBEDTLS)
#elif defined(LIBATFRAME_ATGATEWAY_ENABLE_LIBRESSL)
#endif
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

            return 0;
        }
    }
}