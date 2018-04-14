//
// Created by owent on 2016/10/5.
//

#include <assert.h>
#include <std/thread.h>

#include <log/log_wrapper.h>
#include <common/string_oprs.h>

#include <hiredis/hiredis.h>

#include <protocol/pbdesc/svr.const.err.pb.h>

#include "db_utils.h"

#ifndef PROJECT_RPC_DB_BUFFER_LENGTH
#define PROJECT_RPC_DB_BUFFER_LENGTH 131072
#endif

#if defined(THREAD_TLS_ENABLED) && 1 == THREAD_TLS_ENABLED
namespace rpc {
    namespace db {
        namespace detail {
            char *get_pack_tls_buffer() {
                static THREAD_TLS char ret[PROJECT_RPC_DB_BUFFER_LENGTH];
                return ret;
            }
        }
    }
}

#else

#include <pthread.h>
namespace rpc {
    namespace db {
        namespace detail {
            static pthread_once_t gt_get_pack_tls_once = PTHREAD_ONCE_INIT;
            static pthread_key_t gt_get_pack_tls_key;

            static void dtor_pthread_get_log_tls(void *p) {
                char *buffer_block = reinterpret_cast<char *>(p);
                if (NULL != buffer_block) {
                    delete[] buffer_block;
                }
            }

            static void init_pthread_get_log_tls() { (void)pthread_key_create(&gt_get_pack_tls_key, dtor_pthread_get_log_tls); }

            char *get_pack_tls_buffer() {
                (void)pthread_once(&gt_get_pack_tls_once, init_pthread_get_log_tls);
                char *buffer_block = reinterpret_cast<char *>(pthread_getspecific(gt_get_pack_tls_key));
                if (NULL == buffer_block) {
                    buffer_block = new char[PROJECT_RPC_DB_BUFFER_LENGTH];
                    pthread_setspecific(gt_get_pack_tls_key, buffer_block);
                }
                return buffer_block;
            }
        }
    }
}

#endif

#define RPC_DB_VERSION_NAME "version"
#define RPC_DB_VERSION_LENGTH 7

namespace rpc {
    namespace db {
        redis_args::redis_args(size_t argc) : used_(0), free_buffer_(::rpc::db::detail::get_pack_tls_buffer()) {
            segment_value_.resize(argc);
            segment_length_.resize(argc);
        }

        redis_args::~redis_args() {}

        char *redis_args::alloc(size_t sz) {
            if (used_ >= segment_value_.size()) {
                WLOGERROR("segment number extended");
                assert(false);
                return NULL;
            }

            size_t used_buf_len = free_buffer_ - ::rpc::db::detail::get_pack_tls_buffer();
            if (used_buf_len + sz > PROJECT_RPC_DB_BUFFER_LENGTH) {
                WLOGERROR("buffer length extended before padding");
                assert(false);
                return NULL;
            }

            size_t free_buf_len = PROJECT_RPC_DB_BUFFER_LENGTH - used_buf_len;
            void *start_addr = reinterpret_cast<void *>(free_buffer_);
            if (NULL == align_alloc<size_t>(start_addr, free_buf_len)) {
                WLOGERROR("buffer length extended when padding");
                assert(false);
                return NULL;
            }

            if (free_buf_len < sz) {
                WLOGERROR("buffer length extended after padding");
                assert(false);
                return NULL;
            }

            free_buffer_ = reinterpret_cast<char *>(start_addr) + sz;

            segment_value_[used_] = reinterpret_cast<char *>(start_addr);
            segment_length_[used_] = sz;
            ++used_;
            return reinterpret_cast<char *>(start_addr);
        }

        void redis_args::dealloc() {
            if (0 == used_) {
                return;
            }

            --used_;
            free_buffer_ -= segment_length_[used_];
        }

        bool redis_args::empty() const { return 0 == used_; }

        size_t redis_args::size() const { return used_; }

        const char **redis_args::get_args_values() { return segment_value_.data(); }

        const size_t *redis_args::get_args_lengths() const { return segment_length_.data(); }

        bool redis_args::push(const char *str, size_t len) {
            assert(str);
            if (0 == len) {
                len = strlen(str);
            }

            char *d = alloc(len);
            if (NULL == d) {
                return false;
            }
            memcpy(d, str, len);
            return true;
        }

        bool redis_args::push(const std::string &str) { return push(str.c_str(), str.size()); }

        bool redis_args::push(uint8_t v) {
            char td[12] = {0};
            int sz = UTIL_STRFUNC_SNPRINTF(td, sizeof(td), "%u", static_cast<unsigned int>(v));
            if (sz < 0) {
                WLOGERROR("snprintf failed, res: %d", sz);
                return false;
            }

            char *d = alloc(sz);
            if (NULL == d) {
                return false;
            }
            memcpy(d, td, static_cast<size_t>(sz));
            return true;
        }

        bool redis_args::push(int8_t v) {
            char td[12] = {0};
            int sz = UTIL_STRFUNC_SNPRINTF(td, sizeof(td), "%d", static_cast<int>(v));
            if (sz < 0) {
                WLOGERROR("snprintf failed, res: %d", sz);
                return false;
            }

            char *d = alloc(sz);
            if (NULL == d) {
                return false;
            }
            memcpy(d, td, static_cast<size_t>(sz));
            return true;
        }

        bool redis_args::push(uint16_t v) {
            char td[12] = {0};
            int sz = UTIL_STRFUNC_SNPRINTF(td, sizeof(td), "%u", static_cast<unsigned int>(v));
            if (sz < 0) {
                WLOGERROR("snprintf failed, res: %d", sz);
                return false;
            }

            char *d = alloc(sz);
            if (NULL == d) {
                return false;
            }
            memcpy(d, td, static_cast<size_t>(sz));
            return true;
        }

        bool redis_args::push(int16_t v) {
            char td[12] = {0};
            int sz = UTIL_STRFUNC_SNPRINTF(td, sizeof(td), "%d", static_cast<int>(v));
            if (sz < 0) {
                WLOGERROR("snprintf failed, res: %d", sz);
                return false;
            }

            char *d = alloc(sz);
            if (NULL == d) {
                return false;
            }
            memcpy(d, td, static_cast<size_t>(sz));
            return true;
        }

        bool redis_args::push(uint32_t v) {
            char td[12] = {0};
            int sz = UTIL_STRFUNC_SNPRINTF(td, sizeof(td), "%u", static_cast<unsigned int>(v));
            if (sz < 0) {
                WLOGERROR("snprintf failed, res: %d", sz);
                return false;
            }

            char *d = alloc(sz);
            if (NULL == d) {
                return false;
            }
            memcpy(d, td, static_cast<size_t>(sz));
            return true;
        }

        bool redis_args::push(int32_t v) {
            char td[12] = {0};
            int sz = UTIL_STRFUNC_SNPRINTF(td, sizeof(td), "%d", static_cast<int>(v));
            if (sz < 0) {
                WLOGERROR("snprintf failed, res: %d", sz);
                return false;
            }

            char *d = alloc(sz);
            if (NULL == d) {
                return false;
            }
            memcpy(d, td, static_cast<size_t>(sz));
            return true;
        }

        bool redis_args::push(uint64_t v) {
            char td[24] = {0};
            int sz = UTIL_STRFUNC_SNPRINTF(td, sizeof(td), "%llu", static_cast<unsigned long long>(v));
            if (sz < 0) {
                WLOGERROR("snprintf failed, res: %d", sz);
                return false;
            }

            char *d = alloc(sz);
            if (NULL == d) {
                return false;
            }
            memcpy(d, td, static_cast<size_t>(sz));
            return true;
        }

        bool redis_args::push(int64_t v) {
            char td[24] = {0};
            int sz = UTIL_STRFUNC_SNPRINTF(td, sizeof(td), "%lld", static_cast<long long>(v));
            if (sz < 0) {
                WLOGERROR("snprintf failed, res: %d", sz);
                return false;
            }

            char *d = alloc(sz);
            if (NULL == d) {
                return false;
            }
            memcpy(d, td, static_cast<size_t>(sz));
            return true;
        }

        int unpack_message(::google::protobuf::Message &msg, const redisReply *reply, std::string *version) {
            if (NULL == reply) {
                WLOGDEBUG("unpack message %s failed, data mot found.", msg.GetDescriptor()->full_name().c_str());
                return hello::err::EN_SYS_PARAM;
            }

            bool has_failed = false;
            const ::google::protobuf::Reflection *reflect = msg.GetReflection();
            const google::protobuf::Descriptor *desc = msg.GetDescriptor();

            if (REDIS_REPLY_ARRAY != reply->type) {
                WLOGDEBUG("unpack message %s failed, reply type %d is not a array.", msg.GetDescriptor()->full_name().c_str(), reply->type);
                return hello::err::EN_SYS_UNPACK;
            }

            if (reply->elements <= 0) {
                return hello::err::EN_SUCCESS;
            }

            for (size_t i = 0; i < reply->elements - 1; i += 2) {
                const redisReply *key = reply->element[i];
                const redisReply *value = reply->element[i + 1];

                if (REDIS_REPLY_STRING != key->type || NULL == key->str) {
                    if (NULL != key->str) {
                        WLOGDEBUG("unpack message %s failed, key(replay[%llu], %s) type %d is not a string.", msg.GetDescriptor()->full_name().c_str(),
                                  static_cast<unsigned long long>(i), key->str, key->type);
                    } else {
                        WLOGDEBUG("unpack message %s failed, key(replay[%llu]) type %d is not a string.", msg.GetDescriptor()->full_name().c_str(),
                                  static_cast<unsigned long long>(i), key->type);
                    }
                    continue;
                }

                if (NULL != version && 0 == UTIL_STRFUNC_STRNCMP(RPC_DB_VERSION_NAME, key->str, RPC_DB_VERSION_LENGTH)) {
                    if (REDIS_REPLY_INTEGER == value->type) {
                        char intval[24] = {0};
                        UTIL_STRFUNC_SNPRINTF(intval, sizeof(intval), "%lld", value->integer);
                        version->assign((const char *)intval);
                    } else if (NULL != value->str) {
                        version->assign(value->str);
                    } else {
                        version->assign("0");
                    }

                    version = NULL;
                    continue;
                }

                const ::google::protobuf::FieldDescriptor *fd = desc->FindFieldByName(key->str);
                // 老版本的服务器用新的数据
                if (NULL == fd) {
                    // has_failed = true;
                    WLOGERROR("unpack message %s failed, field name %s not found, maybe deleted", msg.GetDescriptor()->full_name().c_str(), key->str);
                    continue;
                }

#define CASE_REDIS_DATA_TO_PB_INT(pbtype, cpptype, func)                                                                                             \
    case pbtype: {                                                                                                                                   \
        if (REDIS_REPLY_NIL == value->type) {                                                                                                        \
            break;                                                                                                                                   \
        }                                                                                                                                            \
        if (REDIS_REPLY_INTEGER == value->type) {                                                                                                    \
            reflect->func(&msg, fd, static_cast<cpptype>(value->integer));                                                                           \
        } else if (REDIS_REPLY_STRING == value->type && NULL != value->str) {                                                                        \
            cpptype v = 0;                                                                                                                           \
            util::string::str2int(v, value->str);                                                                                                    \
            reflect->func(&msg, fd, v);                                                                                                              \
        } else {                                                                                                                                     \
            WLOGERROR("unpack message %s failed, type of %s in pb is a message, but the redis reply type is not string nor integer(reply type=%d).", \
                      msg.GetDescriptor()->full_name().c_str(), key->str, value->type);                                                              \
            has_failed = true;                                                                                                                       \
        }                                                                                                                                            \
        break;                                                                                                                                       \
    }

                switch (fd->cpp_type()) {
                case google::protobuf::FieldDescriptor::CPPTYPE_STRING: {
                    if (REDIS_REPLY_NIL == value->type) {
                        break;
                    }

                    if (REDIS_REPLY_STRING != value->type || NULL == value->str) {
                        WLOGERROR("unpack message %s failed, type of %s in pb is a string, but the redis reply type is not(reply type=%d).",
                                  msg.GetDescriptor()->full_name().c_str(), key->str, value->type);
                        has_failed = true;
                    } else {
                        reflect->SetString(&msg, fd, value->str);
                    }
                    break;
                }
                case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE: {
                    if (REDIS_REPLY_NIL == value->type) {
                        break;
                    }

                    if (REDIS_REPLY_STRING != value->type || NULL == value->str) {
                        WLOGERROR("unpack message %s failed, type of %s in pb is a message, but the redis reply type is not string(reply type=%d).",
                                  msg.GetDescriptor()->full_name().c_str(), key->str, value->type);
                        has_failed = true;
                    } else {
                        ::google::protobuf::Message *data_msg = reflect->MutableMessage(&msg, fd);
                        if (NULL == data_msg) {
                            has_failed = true;
                            WLOGERROR("mutable message %s.%s failed", msg.GetDescriptor()->full_name().c_str(), key->str);
                            continue;
                        }

                        if (false == data_msg->ParseFromArray(value->str, static_cast<int>(value->len))) {
                            has_failed = true;
                            WLOGERROR("message field [%s] unpack error failed", key->str);
                            WLOGDEBUG("%s", data_msg->InitializationErrorString().c_str());
                        }
                    }

                    break;
                }

                    CASE_REDIS_DATA_TO_PB_INT(google::protobuf::FieldDescriptor::CPPTYPE_INT32, google::protobuf::int32, SetInt32)
                    CASE_REDIS_DATA_TO_PB_INT(google::protobuf::FieldDescriptor::CPPTYPE_INT64, google::protobuf::int64, SetInt64)
                    CASE_REDIS_DATA_TO_PB_INT(google::protobuf::FieldDescriptor::CPPTYPE_UINT32, google::protobuf::uint32, SetUInt32)
                    CASE_REDIS_DATA_TO_PB_INT(google::protobuf::FieldDescriptor::CPPTYPE_UINT64, google::protobuf::uint64, SetUInt64)
                    CASE_REDIS_DATA_TO_PB_INT(google::protobuf::FieldDescriptor::CPPTYPE_ENUM, int, SetEnumValue)

                default: {
                    WLOGERROR("message %s field %s(type=%s) invalid", msg.GetDescriptor()->full_name().c_str(), fd->name().c_str(), fd->cpp_type_name());
                    break;
                }
                }
            }

#undef CASE_REDIS_DATA_TO_PB_INT

            if (has_failed) {
                WLOGERROR("unpack message %s finished, but not all data fields success: %s", msg.GetDescriptor()->full_name().c_str(),
                          msg.DebugString().c_str());
                return hello::err::EN_SYS_UNPACK;
            }

            return hello::err::EN_SUCCESS;
        }


        int pack_message(const ::google::protobuf::Message &msg, redis_args &args, std::vector<const ::google::protobuf::FieldDescriptor *> fds,
                         std::string *version, std::ostream *debug_message) {
            // 反射获取所有的字段
            const google::protobuf::Reflection *reflect = msg.GetReflection();
            if (NULL == reflect) {
                WLOGERROR("pack message %s failed, get reflection failed", msg.GetDescriptor()->full_name().c_str());
                return hello::err::EN_SYS_PACK;
            }

            if (NULL != version) {
                char *d = args.alloc(RPC_DB_VERSION_LENGTH);
                if (NULL == d) {
                    WLOGERROR("pack message %s failed, alloc version key failed", msg.GetDescriptor()->full_name().c_str());
                    return hello::err::EN_SYS_MALLOC;
                }
                memcpy(d, RPC_DB_VERSION_NAME, RPC_DB_VERSION_LENGTH);

                //
                d = args.alloc(version->size());
                if (NULL == d) {
                    args.dealloc();
                    WLOGERROR("pack message %s failed, alloc version value failed", msg.GetDescriptor()->full_name().c_str());
                    return hello::err::EN_SYS_MALLOC;
                }
                memcpy(d, version->c_str(), version->size());
            }

            size_t stat_sum_len = 0;
            for (size_t i = 0; i < fds.size(); ++i) {
                if (NULL == fds[i]) {
                    continue;
                }

                char *data_allocated = args.alloc(fds[i]->name().size());
                if (NULL == data_allocated) {
                    WLOGERROR("pack message %s failed, alloc %s key failed", msg.GetDescriptor()->full_name().c_str(), fds[i]->name().c_str());
                    return hello::err::EN_SYS_MALLOC;
                }
                memcpy(data_allocated, fds[i]->name().c_str(), fds[i]->name().size());

#define CASE_PB_INT_TO_REDIS_DATA(pbtype, cpptype, cppformat, func)                                                                                      \
    case pbtype: {                                                                                                                                       \
        cpptype vint = static_cast<cpptype>(reflect->func(msg, fds[i]));                                                                                 \
        char vstr[24] = {0};                                                                                                                             \
        int intlen = UTIL_STRFUNC_SNPRINTF(vstr, sizeof(vstr), cppformat, vint);                                                                         \
        data_allocated = args.alloc(static_cast<size_t>(intlen));                                                                                        \
        if (NULL == data_allocated || intlen < 0) {                                                                                                      \
            WLOGERROR("pack message %s failed, alloc %s,len=%d value failed", msg.GetDescriptor()->full_name().c_str(), fds[i]->name().c_str(), intlen); \
            args.dealloc();                                                                                                                              \
            return hello::err::EN_SYS_MALLOC;                                                                                                            \
        }                                                                                                                                                \
        memcpy(data_allocated, vstr, static_cast<size_t>(intlen));                                                                                       \
        stat_sum_len += static_cast<size_t>(intlen);                                                                                                     \
        if (NULL != debug_message) {                                                                                                                     \
            (*debug_message) << fds[i]->name() << "=" << vint << ",";                                                                                    \
        }                                                                                                                                                \
        break;                                                                                                                                           \
    }


                switch (fds[i]->cpp_type()) {
                // 字符串直接保存
                case google::protobuf::FieldDescriptor::CPPTYPE_STRING: {
                    const std::string *seg_val;
                    std::string empty;
                    if (reflect->HasField(msg, fds[i])) {
                        seg_val = &reflect->GetStringReference(msg, fds[i], NULL);
                    } else {
                        seg_val = &empty;
                    }

                    // 再dump 字段内容
                    data_allocated = args.alloc(seg_val->size());
                    if (NULL == data_allocated) {
                        WLOGERROR("pack message %s failed, alloc %s value failed", msg.GetDescriptor()->full_name().c_str(), fds[i]->name().c_str());

                        args.dealloc();
                        return hello::err::EN_SYS_MALLOC;
                    }
                    memcpy(data_allocated, seg_val->data(), seg_val->size());

                    stat_sum_len += seg_val->size();
                    if (NULL != debug_message) {
                        (*debug_message) << fds[i]->name() << "=" << *seg_val << ",";
                    }
                    break;
                }

                // message需要序列化
                case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE: {
                    const ::google::protobuf::Message &seg_val = reflect->GetMessage(msg, fds[i]);
                    size_t dump_len = static_cast<size_t>(seg_val.ByteSize());

                    data_allocated = args.alloc(dump_len);
                    if (NULL == data_allocated) {
                        WLOGERROR("pack message %s failed, alloc %s value failed", msg.GetDescriptor()->full_name().c_str(), fds[i]->name().c_str());

                        args.dealloc();
                        return hello::err::EN_SYS_MALLOC;
                    }

                    // 再dump 字段内容
                    seg_val.SerializeWithCachedSizesToArray(reinterpret_cast< ::google::protobuf::uint8 *>(data_allocated));


                    stat_sum_len += dump_len;
                    if (NULL != debug_message) {
                        (*debug_message) << fds[i]->name() << "=" << dump_len << " bytes,";
                    }
                    break;
                };

                    // 整数类型
                    CASE_PB_INT_TO_REDIS_DATA(google::protobuf::FieldDescriptor::CPPTYPE_INT32, int, "%d", GetInt32)
                    CASE_PB_INT_TO_REDIS_DATA(google::protobuf::FieldDescriptor::CPPTYPE_INT64, long long, "%lld", GetInt64)
                    CASE_PB_INT_TO_REDIS_DATA(google::protobuf::FieldDescriptor::CPPTYPE_UINT32, unsigned int, "%u", GetUInt32)
                    CASE_PB_INT_TO_REDIS_DATA(google::protobuf::FieldDescriptor::CPPTYPE_UINT64, unsigned long long, "%llu", GetUInt64)
                    CASE_PB_INT_TO_REDIS_DATA(google::protobuf::FieldDescriptor::CPPTYPE_ENUM, int, "%d", GetEnumValue)

                default: {
                    WLOGERROR("message %s field %s(type=%s) invalid", msg.GetDescriptor()->full_name().c_str(), fds[i]->name().c_str(),
                              fds[i]->cpp_type_name());
                    args.dealloc();
                    break;
                }
                }

#undef CASE_PB_INT_TO_REDIS_DATA
            }

            if (NULL != debug_message) {
                (*debug_message) << ". total value length=" << stat_sum_len << " bytes";
            }

            return hello::err::EN_SUCCESS;
        }
    }
}