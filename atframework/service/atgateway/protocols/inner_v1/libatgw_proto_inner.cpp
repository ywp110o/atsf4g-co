#include "libatgw_proto_inner.h"

namespace atframe {
    namespace gateway {
        namespace detail {
            static uint64_t alloc_seq() {
                static seq_alloc_u64 seq_alloc;
                uint64_t ret = seq_alloc.inc();
                while (0 == ret) {
                    ret = seq_alloc.inc();
                }
                return ret;
            }
        }
        libatgw_proto_inner_v1::libatgw_proto_inner_v1() : session_id_(0), last_write_ptr_(NULL), close_reason_(0) {
            crypt_info_.type = ::atframe::gw::inner::v1::crypt_type_t_EN_ET_NONE;
            crypt_info_.keybits = 0;

            read_head_.len = 0;

            ping_.last_ping = 0;
            ping_.last_delta = 0;
        }


        libatgw_proto_inner_v1::~libatgw_proto_inner_v1() { close(close_reason_t::EN_CRT_UNKNOWN, false); }

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

        void libatgw_proto_inner_v1::dispatch_data(const char *buffer, size_t len, int status, int errcode) {
            if (check_flag(flag_t::EN_PFT_CLOSING)) {
                return error_code_t::EN_ECT_CLOSING;
            }

            // do nothing if any error
            if (errcode < 0 || NULL == buffer) {
                return;
            }

            // verify
            if (false == atframe::gw::inner::v1::Verify(::flatbuffers::Verifier(buffer, len))) {
                close(close_reason_t::EN_CRT_INVALID_DATA);
                return;
            }

            // unpack
            const atframe::gw::inner::v1::cs_msg *msg = Getcs_msg(buffer);
            if (NULL == msg->head()) {
                return;
            }

            switch (msg->head()->type()) {
            case atframe::gw::inner::v1::cs_msg_type_t_EN_MTT_POST: {
                if (::atframe::gw::inner::v1::cs_msg_body_cs_body_post != msg->body_type()) {
                    close(close_reason_t::EN_CRT_INVALID_DATA, false);
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
                        callbacks_->message_fn(this, out, outsz);
                    }
                } else {
                    close(close_reason_t::EN_CRT_INVALID_DATA, false);
                }

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
            case atframe::gw::inner::v1::cs_msg_type_t_EN_MTT_POST_KEY_SYN: {
                if (::atframe::gw::inner::v1::cs_msg_body_cs_body_post != msg->body_type()) {
                    close(close_reason_t::EN_CRT_INVALID_DATA, false);
                    break;
                }

                const ::atframe::gw::inner::v1::cs_body_post *msg_body =
                    static_cast<const ::atframe::gw::inner::v1::cs_body_post *>(msg->body());

                const void *out;
                size_t outsz = static_cast<size_t>(msg_body->length());
                int res = decode_post(msg_body->data()->data(), static_cast<size_t>(msg_body->data()->size()), out, outsz);
                if (0 == res) {
                    send_key_ack(out, outsz);
                } else {
                    close(close_reason_t::EN_CRT_INVALID_DATA, false);
                }
                break;
            }
            case atframe::gw::inner::v1::cs_msg_type_t_EN_MTT_POST_KEY_ACK: {
                if (::atframe::gw::inner::v1::cs_msg_body_cs_body_post != msg->body_type()) {
                    close(close_reason_t::EN_CRT_INVALID_DATA, false);
                    break;
                }

                const ::atframe::gw::inner::v1::cs_body_post *msg_body =
                    static_cast<const ::atframe::gw::inner::v1::cs_body_post *>(msg->body());

                const void *out;
                size_t outsz = static_cast<size_t>(msg_body->length());
                int res = decode_post(msg_body->data()->data(), static_cast<size_t>(msg_body->data()->size()), out, outsz);
                if (0 == res) {
                    // TODO verify and use the new secret now
                } else {
                    close(close_reason_t::EN_CRT_INVALID_DATA, false);
                }

                break;
            }
            default: { break; }
            }
        }

        int libatgw_proto_inner_v1::dispatch_handshake(const ::atframe::gw::inner::v1::cs_body_handshake &body_handshake) {
            if (check_flag(flag_t::EN_PFT_CLOSING)) {
                return error_code_t::EN_ECT_CLOSING;
            }

            if (check_flag(ext_flag_t::EN_PEFT_HANDSHAKE_DONE)) {
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
            return ret;
        }

        int libatgw_proto_inner_v1::dispatch_handshake_start_req(const ::atframe::gw::inner::v1::cs_body_handshake &body_handshake) {
            if (check_flag(flag_t::EN_PFT_CLOSING)) {
                return error_code_t::EN_ECT_CLOSING;
            }

            if (NULL == callbacks_ || !callbacks_->write_fn) {
                return error_code_t::EN_ECT_MISS_CALLBACKS;
            }

            uint64_t sess_id = 0;
            if (!callbacks_->new_session_fn) {
                !callbacks_->new_session_fn(this, sess_id);
            }

            flatbuffers::FlatBufferBuilder builder;
            ::atframe::gw::inner::v1::cs_msgBuilder msg(builder);
            msg.add_head(::atframe::gw::inner::v1::Createcs_msg_head(builder, ::atframe::gw::inner::v1::cs_msg_type_t_EN_MTT_HANDSHAKE,
                                                                     ::atframe::gateway::detail::alloc_seq()));
            ::atframe::gw::inner::v1::cs_body_handshakeBuilder handshake_body(builder);


            handshake_body.add_session_id(sess_id);
            handshake_body.add_step(::atframe::gw::inner::v1::handshake_step_t_EN_HST_START_RSP);

            if (0 != sess_id) {
                // TODO use the global switch type
                handshake_body.add_switch_type(::atframe::gw::inner::v1::switch_secret_t_EN_SST_DIRECT);
                // TODO use the global crypt type
                // crypt_info_.type = ;
                // crypt_info_.secret = ;
                // crypt_info_.keybits = ;
                handshake_body.add_crypt_type(static_cast< ::atframe::gw::inner::v1::crypt_type_t>(crypt_info_.type));
                handshake_body.add_crypt_bits(crypt_info_.keybits);
                handshake_body.add_crypt_param(builder.CreateVector(crypt_info_.secret.data(), crypt_info_.secret.size()));

                // if switch type is RSA
                ::atframe::gw::inner::v1::cs_body_rsa_certBuilder rsa_cert(builder);
                rsa_cert.add_rsa_sign(::atframe::gw::inner::v1::rsa_sign_t_EN_RST_PKCS1);
                rsa_cert.add_hash_type(::atframe::gw::inner::v1::hash_id_t_EN_HIT_MD5);
                rsa_cert.add_pubkey(builder.CreateVector(NULL, 0));
                handshake_body.add_rsa_cert(rsa_cert.Finish());
            }

            msg.add_body_type(::atframe::gw::inner::v1::cs_msg_body_cs_body_handshake);
            msg.add_body(handshake_body.Finish());

            msg.Finish();
            builder.Finish();
            return write_msg(builder);
        }
        int libatgw_proto_inner_v1::dispatch_handshake_start_rsp(const ::atframe::gw::inner::v1::cs_body_handshake &body_handshake) {
            // TODO check if start new session success
            // TODO assign session id,
            // TODO if not use crypt, assign crypt information and set_handshake_done(0)

            // TODO if in DH handshake, generate and send pubkey
            // TODO if in RSA handshake, generate and send secret
            return 0;
        }
        int libatgw_proto_inner_v1::dispatch_handshake_reconn_req(const ::atframe::gw::inner::v1::cs_body_handshake &body_handshake) {
            // TODO assign crypt options
            // TODO try to reconnect
            return 0;
        }
        int libatgw_proto_inner_v1::dispatch_handshake_reconn_rsp(const ::atframe::gw::inner::v1::cs_body_handshake &body_handshake) {
            return 0;
        }
        int libatgw_proto_inner_v1::dispatch_handshake_dh_pubkey_req(const ::atframe::gw::inner::v1::cs_body_handshake &body_handshake) {
            return 0;
        }
        int libatgw_proto_inner_v1::dispatch_handshake_dh_pubkey_rsp(const ::atframe::gw::inner::v1::cs_body_handshake &body_handshake) {
            return 0;
        }
        int libatgw_proto_inner_v1::dispatch_handshake_rsa_secret_req(const ::atframe::gw::inner::v1::cs_body_handshake &body_handshake) {
            return 0;
        }
        int libatgw_proto_inner_v1::dispatch_handshake_rsa_secret_rsp(const ::atframe::gw::inner::v1::cs_body_handshake &body_handshake) {
            return 0;
        }
        int libatgw_proto_inner_v1::dispatch_handshake_verify_ntf(const ::atframe::gw::inner::v1::cs_body_handshake &body_handshake) {
            return 0;
        }

        int libatgw_proto_inner_v1::set_handshake_done(int status) {
            if (check_flag(ext_flag_t::EN_PEFT_HANDSHAKE_DONE)) {
                return error_code_t::EN_ECT_HANDSHAKE;
            }
            set_flag(ext_flag_t::EN_PEFT_HANDSHAKE_DONE, true);

            if (NULL != callbacks_ && callbacks_->on_handshake_done_fn) {
                callbacks_->on_handshake_done_fn(this, status);
            }

            return 0;
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
            const size_t msg_header_len = sizeof(uint32_t) + sizeof(uint32_t);

            // closing or closed, cancle writing
            if (check_flag(flags_, flag_t::EN_PFT_CLOSING)) {
                while (!write_buffers_.empty()) {
                    // ::atbus::detail::buffer_block *bb = write_buffers_.front();
                    // size_t nwrite = bb->raw_size();
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
            void *buffer = NULL;
            size_t sz = 0;
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
                connection->write_buffers.pop_front(writing_block->raw_size(), true);
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
            const size_t msg_header_len = sizeof(uint32_t) + sizeof(uint32_t);

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
            if (check_flag(flag_t::EN_PFT_CLOSING)) {
                return false;
            }

            libatgw_proto_inner_v1 *other_proto = dynamic_cast<libatgw_proto_inner_v1 *>(other);
            assert(other_proto);

            // TODO decrypt secret

            // check crypt type, keybits and encrypted secret
            if (!(crypt_info_.type == other_proto->crypt_info_.type && crypt_info_.keybits == other_proto->crypt_info_.keybits &&
                  crypt_info_.secret == other_proto->crypt_info_.secret)) {
                return false;
            }


            // if success, copy crypt information
            session_id_ = other_proto->session_id_;
            crypt_info_ = other_proto->crypt_info_;
            return true;
        }

        int libatgw_proto_inner_v1::start_session() {
            if (check_flag(flag_t::EN_PFT_CLOSING)) {
                return error_code_t::EN_ECT_CLOSING;
            }

            if (NULL == callbacks_ || !callbacks_->write_fn) {
                return error_code_t::EN_ECT_MISS_CALLBACKS;
            }

            if (0 != session_id_) {
                return error_code_t::EN_ECT_SESSION_ALREADY_EXIST;
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
            handshake_body.add_crypt_param(builder.CreateVector(NULL, 0));

            ::atframe::gw::inner::v1::cs_body_rsa_certBuilder rsa_cert(builder);
            rsa_cert.add_rsa_sign(::atframe::gw::inner::v1::rsa_sign_t_EN_RST_PKCS1);
            rsa_cert.add_hash_type(::atframe::gw::inner::v1::hash_id_t_EN_HIT_MD5);
            rsa_cert.add_pubkey(builder.CreateVector(NULL, 0));
            handshake_body.add_rsa_cert(rsa_cert.Finish());

            msg.add_body_type(::atframe::gw::inner::v1::cs_msg_body_cs_body_handshake);
            msg.add_body(handshake_body.Finish());

            msg.Finish();
            builder.Finish();
            return write_msg(builder);
        }

        int libatgw_proto_inner_v1::reconnect_session(uint64_t sess_id, int type, const std::string &secret, uint32_r keybits) {
            if (check_flag(flag_t::EN_PFT_CLOSING)) {
                return error_code_t::EN_ECT_CLOSING;
            }

            if (NULL == callbacks_ || !callbacks_->write_fn) {
                return error_code_t::EN_ECT_MISS_CALLBACKS;
            }

            if (0 != session_id_) {
                return error_code_t::EN_ECT_SESSION_ALREADY_EXIST;
            }

            // TODO encrypt secret
            const void *secret_buffer = NULL;
            size_t secret_length = secret.size();

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
            handshake_body.add_crypt_param(builder.CreateVector(secret_buffer, secret_length));

            ::atframe::gw::inner::v1::cs_body_rsa_certBuilder rsa_cert(builder);
            rsa_cert.add_rsa_sign(::atframe::gw::inner::v1::rsa_sign_t_EN_RST_PKCS1);
            rsa_cert.add_hash_type(::atframe::gw::inner::v1::hash_id_t_EN_HIT_MD5);
            rsa_cert.add_pubkey(builder.CreateVector(secret.data(), secret.size()));
            handshake_body.add_rsa_cert(rsa_cert.Finish());

            msg.add_body_type(::atframe::gw::inner::v1::cs_msg_body_cs_body_handshake);
            msg.add_body(handshake_body.Finish());

            msg.Finish();
            builder.Finish();
            return write_msg(builder);
        }

        int libatgw_proto_inner_v1::send_post(::atframe::gw::inner::v1::cs_msg_type_t msg_type, const void *buffer, size_t len) {
            if (check_flag(flag_t::EN_PFT_CLOSING)) {
                return error_code_t::EN_ECT_CLOSING;
            }

            if (NULL == callbacks_ || !callbacks_->write_fn) {
                return error_code_t::EN_ECT_MISS_CALLBACKS;
            }


            // TODO encrypt
            // TODO zip
            // TODO push data
            // TODO if not writing, try merge data and write it
            flatbuffers::FlatBufferBuilder builder;
            ::atframe::gw::inner::v1::cs_msgBuilder msg(builder);
            msg.add_head(::atframe::gw::inner::v1::Createcs_msg_head(builder, msg_type, ::atframe::gateway::detail::alloc_seq()));
            ::atframe::gw::inner::v1::cs_body_postBuilder post_body(builder);

            post_body.add_length(builder.CreateString(static_cast<size_t>(len)));
            post_body.add_data(builder.CreateString(reinterpret_cast<const char *>(buffer), static_cast<size_t>(len)));

            msg.add_body_type(::atframe::gw::inner::v1::cs_msg_body_cs_body_post);
            msg.add_body(post_body.Finish());

            msg.Finish();
            builder.Finish();
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
            msg.add_body(ping_body.Finish());

            msg.Finish();
            builder.Finish();
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
            msg.add_body(ping_body.Finish());

            msg.Finish();
            builder.Finish();
            return write_msg(builder);
        }

        int libatgw_proto_inner_v1::send_key_syn(const void *secret, size_t len) {
            if (check_flag(flag_t::EN_PFT_CLOSING)) {
                return error_code_t::EN_ECT_CLOSING;
            }

            // TODO mark next secret
            return send_post(::atframe::gw::inner::v1::cs_msg_type_t_EN_MTT_POST_KEY_SYN, secret, len);
        }

        int libatgw_proto_inner_v1::send_key_ack(const void *secret, size_t len) {
            if (check_flag(flag_t::EN_PFT_CLOSING)) {
                return error_code_t::EN_ECT_CLOSING;
            }

            // TODO update secret
            return send_post(::atframe::gw::inner::v1::cs_msg_type_t_EN_MTT_POST_KEY_ACK, secret, len);
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
            msg.add_body(kickoff_body.Finish());

            msg.Finish();
            builder.Finish();
            return write_msg(builder);
        }

        int libatgw_proto_inner_v1::decode_post(const void *in, size_t insz, const void *&out, size_t &outsz) {
            if (check_flag(flag_t::EN_PFT_CLOSING)) {
                return error_code_t::EN_ECT_CLOSING;
            }

            // TODO unzip
            // TODO decrypt

            outsz = insz;
            out = in;
            return 0;
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