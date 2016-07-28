#ifndef LIBATBUS_PROTOCOL_DESC_H_
#define LIBATBUS_PROTOCOL_DESC_H_

#pragma once

#include <cstddef>
#include <ostream>
#include <stdint.h>

#include <msgpack.hpp>

enum ATFRAME_GW_SERVER_PROTOCOL_CMD {
    ATFRAME_GW_CMD_INVALID = 0,

    //  数据协议
    ATFRAME_GW_CMD_POST = 1,

    // 节点控制协议
    ATFRAME_GW_CMD_SESSION_ADD = 11,
    ATFRAME_GW_CMD_SESSION_REMOVE = 12,
    ATFRAME_GW_CMD_SESSION_KICKOFF = 14,
    ATFRAME_GW_CMD_SET_ROUTER_REQ = 15,
    ATFRAME_GW_CMD_SET_ROUTER_RSP = 16,

    ATFRAME_GW_CMD_MAX
};

MSGPACK_ADD_ENUM(ATFRAME_GW_SERVER_PROTOCOL_CMD);

namespace atframe {
    namespace gw {
        struct bin_data_block {
            const void *ptr;
            size_t size;

            template <typename CharT, typename Traits>
            friend std::basic_ostream<CharT, Traits> &operator<<(std::basic_ostream<CharT, Traits> &os, const bin_data_block &mbc) {
                if (NULL != mbc.ptr && mbc.size > 0) {
                    os.write(reinterpret_cast<const CharT *>(mbc.ptr), mbc.size / sizeof(CharT));
                }
                return os;
            }
        };

        struct ss_body_session {
            std::string client_ip; // ID: 0
            int32_t client_port;   // ID: 1

            session() : client_port(0) {}

            MSGPACK_DEFINE(client_ip, client_port);

            template <typename CharT, typename Traits>
            friend std::basic_ostream<CharT, Traits> &operator<<(std::basic_ostream<CharT, Traits> &os, const session &mbc) {
                os << "{" << std::endl
                   << "      client_ip: " << mbc.client_ip << std::endl
                   << "      client_port: " << mbc.client_port << std::endl;
                os << "    }";

                return os;
            }
        };

        struct ss_body_post {
            std::vector<uint64_t> session_ids; // 多播目标, ID: 0
            bin_data_block content;            // ID: 1

            ss_body_post() {
                content.size = 0;
                content.ptr = NULL;
            }

            MSGPACK_DEFINE(session_ids, content);

            template <typename CharT, typename Traits>
            friend std::basic_ostream<CharT, Traits> &operator<<(std::basic_ostream<CharT, Traits> &os, const ss_body_post &mbc) {
                os << "{" << std::endl;
                if (!mbc.session_ids.empty()) {
                    os << "      session_ids: ";
                    for (size_t i = 0; i < mbc.session_ids.size(); ++i) {
                        if (0 != i) {
                            os << ", ";
                        }
                        os << mbc.session_ids[i];
                    }
                    os << std::endl;
                }

                os << "      content: " << mbc.content << std::endl;
                os << "    }";

                return os;
            }
        };

        class ss_msg_body {
        public:
            union {
                ss_body_post *post;
                ss_body_session *session;
                uint64_t router;
            };

            ss_msg_body() {
                post = NULL;
                session = NULL;
                router = 0;
            }
            ~ss_msg_body() {
                if (NULL != post) {
                    delete post;
                }

                if (NULL != session) {
                    delete session;
                }
            }

            template <typename TPtr>
            TPtr *make_body(TPtr *&p) {
                if (NULL != p) {
                    return p;
                }

                return p = new TPtr();
            }

            ss_body_post *make_post(const void *buffer, size_t s) {
                ss_body_post *ret = make_body(post);
                if (NULL == ret) {
                    return ret;
                }

                ret->session_ids.clear();
                ret->content.ptr = buffer;
                ret->content.size = s;
                return ret;
            }

            ss_body_session *make_session(const std::string &ip, int32_t port) {
                ss_body_session *ret = make_body(session);
                if (NULL == ret) {
                    return ret;
                }

                ret->client_ip = ip;
                ret->client_port = port;
                return ret;
            }

            template <typename CharT, typename Traits>
            friend std::basic_ostream<CharT, Traits> &operator<<(std::basic_ostream<CharT, Traits> &os, const msg_body &mb) {
                os << "{" << std::endl;

                if (NULL != mb.post) {
                    os << "    post:" << *mb.post << std::endl;
                }

                if (NULL != mb.session) {
                    os << "    session:" << *mb.session << std::endl;
                }

                os << "  }";

                return os;
            }

        private:
            ss_msg_body(const msg_body &);
            ss_msg_body &operator=(const msg_body &);
        };

        struct ss_msg_head {
            ATFRAME_GW_SERVER_PROTOCOL_CMD cmd; // ID: 0
            uint64_t session_id;                // ID: 1
            int error_code;                     // ID: 2

            msg_head() : cmd(ATFRAME_GW_CMD_INVALID), session_id(0), error_code(0) {}

            MSGPACK_DEFINE(cmd, session_id, error_code);


            template <typename CharT, typename Traits>
            friend std::basic_ostream<CharT, Traits> &operator<<(std::basic_ostream<CharT, Traits> &os, const msg_head &mh) {
                os << "{" << std::endl << "    cmd: " << mh.cmd << std::endl << "    session_id: " << mh.session_id << std::endl << "  }";

                return os;
            }
        };

        struct ss_msg {
            ss_msg_head head; // map.key = 1
            ss_msg_body body; // map.key = 2

            void init(ATFRAME_GW_SERVER_PROTOCOL_CMD cmd, uint64_t session_id) {
                head.cmd = cmd;
                head.session_id = session_id;
            }

            template <typename CharT, typename Traits>
            friend std::basic_ostream<CharT, Traits> &operator<<(std::basic_ostream<CharT, Traits> &os, const msg &m) {
                os << "{" << std::endl << "  head: " << m.head << std::endl << "  body:" << m.body << std::endl << "}";

                return os;
            }
        };
    }
}


// User defined class template specialization
namespace msgpack {
    MSGPACK_API_VERSION_NAMESPACE(MSGPACK_DEFAULT_API_NS) {
        namespace adaptor {

            template <>
            struct convert<atframe::gw::bin_data_block> {
                msgpack::object const &operator()(msgpack::object const &o, atframe::gw::bin_data_block &v) const {
                    if (o.type != msgpack::type::BIN) throw msgpack::type_error();

                    v.ptr = o.via.bin.ptr;
                    v.size = o.via.bin.size;
                    return o;
                }
            };

            template <>
            struct pack<atframe::gw::bin_data_block> {
                template <typename Stream>
                packer<Stream> &operator()(msgpack::packer<Stream> &o, atframe::gw::bin_data_block const &v) const {
                    o.pack_bin(static_cast<uint32_t>(v.size));
                    o.pack_bin_body(reinterpret_cast<const char *>(v.ptr), static_cast<uint32_t>(v.size));
                    return o;
                }
            };

            template <>
            struct object_with_zone<atframe::gw::bin_data_block> {
                void operator()(msgpack::object::with_zone &o, atframe::gw::bin_data_block const &v) const {
                    o.type = type::BIN;
                    o.via.bin.size = static_cast<uint32_t>(v.size);
                    o.via.bin.ptr = reinterpret_cast<const char *>(v.ptr);
                }
            };

            template <>
            struct convert<atframe::gw::msg> {
                msgpack::object const &operator()(msgpack::object const &o, atframe::gw::ss_msg &v) const {
                    if (o.type != msgpack::type::MAP) throw msgpack::type_error();
                    msgpack::object body_obj;
                    // just like protobuf buffer
                    for (uint32_t i = 0; i < o.via.map.size; ++i) {
                        if (o.via.map.ptr[i].key.via.u64 == 1) {
                            o.via.map.ptr[i].val.convert(v.head);
                        } else if (o.via.map.ptr[i].key.via.u64 == 2) {
                            body_obj = o.via.map.ptr[i].val;
                        }
                    }


                    // unpack body using head.cmd
                    if (!body_obj.is_nil()) {
                        switch (v.head.cmd) {

                        case ATFRAME_GW_CMD_POST: {
                            body_obj.convert(*v.body.make_body(v.body.post));
                            break;
                        }

                        case ATFRAME_GW_CMD_SESSION_ADD: {
                            body_obj.convert(*v.body.make_body(v.body.session));
                            break;
                        }

                        default: { // invalid cmd
                            break;
                        }
                        }
                    }

                    return o;
                }
            };

            template <>
            struct pack<atframe::gw::ss_msg> {
                template <typename Stream>
                packer<Stream> &operator()(msgpack::packer<Stream> &o, atframe::gw::ss_msg const &v) const {
                    // packing member variables as an map.
                    o.pack_map(2);
                    o.pack(1);
                    o.pack(v.head);

                    // pack body using head.cmd
                    o.pack(2);
                    switch (v.head.cmd) {

                    case ATFRAME_GW_CMD_POST: {
                        if (NULL == v.body.post) {
                            o.pack_nil();
                        } else {
                            o.pack(*v.body.post);
                        }
                        break;
                    }

                    case ATFRAME_GW_CMD_SESSION_ADD: {
                        if (NULL == v.body.session) {
                            o.pack_nil();
                        } else {
                            o.pack(*v.body.session);
                        }
                        break;
                    }

                    default: { // just cmd
                        break;
                    }
                    }
                    return o;
                }
            };

            template <>
            struct object_with_zone<atframe::gw::ss_msg> {
                void operator()(msgpack::object::with_zone &o, atframe::gw::ss_msg const &v) const {
                    o.type = type::MAP;
                    o.via.map.size = 2;
                    o.via.map.ptr = static_cast<msgpack::object_kv *>(o.zone.allocate_align(sizeof(msgpack::object_kv) * o.via.map.size));

                    o.via.map.ptr[0] = msgpack::object_kv();
                    o.via.map.ptr[0].key = msgpack::object(1);
                    v.head.msgpack_object(&o.via.map.ptr[0].val, o.zone);

                    // pack body using head.cmd
                    o.via.map.ptr[1].key = msgpack::object(1);
                    switch (v.head.cmd) {

                    case ATFRAME_GW_CMD_POST: {
                        if (NULL == v.body.post) {
                            o.via.map.ptr[1].val = msgpack::object();
                        } else {
                            v.body.post->msgpack_object(&o.via.map.ptr[1].val, o.zone);
                        }
                        break;
                    }

                    case ATFRAME_GW_CMD_SESSION_ADD: {
                        if (NULL == v.body.session) {
                            o.via.map.ptr[1].val = msgpack::object();
                        } else {
                            v.body.session->msgpack_object(&o.via.map.ptr[1].val, o.zone);
                        }
                        break;
                    }

                    default: { // invalid cmd
                        break;
                    }
                    }
                }
            };

        } // namespace adaptor
    }     // MSGPACK_API_VERSION_NAMESPACE(MSGPACK_DEFAULT_API_NS)
} // namespace msgpack

#endif // LIBATBUS_PROTOCOL_DESC_H_
