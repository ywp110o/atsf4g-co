//
// Created by owt50 on 2016/9/28.
//

#include <common/string_oprs.h>
#include <log/log_wrapper.h>


#include "unpack.h"

#include <protocol/pbdesc/svr.const.err.pb.h>
#include <protocol/pbdesc/svr.table.pb.h>


#include <hiredis/hiredis.h>

namespace rpc {
    namespace db {
        namespace detail {
            int32_t do_nothing(hello::table_all_message &msg, const redisReply *data) { return hello::err::EN_SUCCESS; }

            int32_t unpack_integer(hello::table_all_message &msg, const redisReply *data) {
                if (NULL == data) {
                    WLOGDEBUG("data mot found.");
                    // 数据找不到，直接成功结束，外层会判为无数据
                    return hello::err::EN_SUCCESS;
                }

                if (REDIS_REPLY_STRING == data->type) {
                    // 坑爹的redis的数据库回包可能回字符串类型
                    int64_t d = 0;
                    util::string::str2int(d, data->str);
                    msg.mutable_simple()->set_msg_i64(d);
                } else if (REDIS_REPLY_INTEGER == data->type) {
                    msg.mutable_simple()->set_msg_i64(data->integer);
                } else {
                    WLOGERROR("data type error, type=%d", data->type);
                    return hello::err::EN_SYS_PARAM;
                }

                return hello::err::EN_SUCCESS;
            }

            int32_t unpack_str(hello::table_all_message &msg, const redisReply *data) {
                if (NULL == data) {
                    WLOGDEBUG("data mot found.");
                    // 数据找不到，直接成功结束，外层会判为无数据
                    return hello::err::EN_SUCCESS;
                }

                if (REDIS_REPLY_STRING != data->type && REDIS_REPLY_STATUS != data->type && REDIS_REPLY_ERROR != data->type) {
                    WLOGERROR("data type error, type=%d", data->type);
                    return hello::err::EN_SYS_PARAM;
                }

                msg.mutable_simple()->set_msg_str(data->str, data->len);
                return hello::err::EN_SUCCESS;
            }

            int32_t unpack_arr_str(hello::table_all_message &msg, const redisReply *data) {
                if (NULL == data) {
                    WLOGDEBUG("data mot found.");
                    // 数据找不到，直接成功结束，外层会判为无数据
                    return hello::err::EN_SUCCESS;
                }

                if (REDIS_REPLY_ARRAY != data->type) {
                    WLOGERROR("data type error, type=%d", data->type);
                    return hello::err::EN_SYS_PARAM;
                }

                hello::table_simple_info *simple_info = msg.mutable_simple();
                for (size_t i = 0; i < data->elements; ++i) {
                    const redisReply *subr = data->element[i];
                    if (REDIS_REPLY_STRING != subr->type && REDIS_REPLY_STATUS != subr->type && REDIS_REPLY_ERROR != subr->type) {
                        continue;
                    }

                    simple_info->add_arr_str(subr->str, subr->len);
                }

                return hello::err::EN_SUCCESS;
            }
        } // namespace detail
    }     // namespace db
} // namespace rpc