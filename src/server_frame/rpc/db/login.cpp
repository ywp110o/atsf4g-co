//
// Created by owt50 on 2016/9/28.
//

#include <log/log_wrapper.h>

#include <protocol/pbdesc/svr.container.pb.h>
#include <protocol/pbdesc/svr.const.err.pb.h>

#include <hiredis/hiredis.h>

#include <dispatcher/task_manager.h>
#include <dispatcher/db_msg_dispatcher.h>
#include <config/logic_config.h>

#include "../rpc_utils.h"
#include "../rpc_macros.h"
#include "db_utils.h"
#include "db_macros.h"
#include "login.h"

namespace rpc {
    namespace db {
        namespace login {

            namespace detail {
                static int32_t unpack_login(hello::message_container &msgc, const redisReply *reply) {
                    if (NULL == reply) {
                        WLOGDEBUG("data not found.");
                        //数据找不到，直接成功结束，外层会判断为无数据
                        return hello::err::EN_SUCCESS;
                    }

                    return ::rpc::db::unpack_message(*msgc.mutable_dbmsg()->mutable_login(), reply, msgc.mutable_dbmsg()->mutable_version());
                }
            }

            int get(const char *openid, hello::table_login &rsp, std::string &version) {
                task_manager::task_t *task = task_manager::task_t::this_task();
                if (!task) {
                    WLOGERROR("current not in a task");
                    return hello::err::EN_SYS_RPC_NO_TASK;
                }

                DBLOGINKEY(user_key, writen_len, openid);
                if (writen_len <= 0) {
                    WLOGERROR("format db cmd failed, cmd %s", user_key);
                    return hello::err::EN_DB_SEND_FAILED;
                }

                int res = db_msg_dispatcher::me()->send_msg(db_msg_dispatcher::channel_t::CLUSTER_DEFAULT, user_key, writen_len, task->get_id(),
                                                            logic_config::me()->get_self_bus_id(), detail::unpack_login, DBLOGINCMD(HGETALL, openid, ""));

                if (res < 0) {
                    return res;
                }

                hello::message_container msgc;
                // 协程操作
                res = rpc::wait(msgc);
                if (res < 0) {
                    return res;
                }

                if (!msgc.has_dbmsg() || !msgc.dbmsg().has_login() || msgc.dbmsg().version().empty()) {
                    return hello::err::EN_DB_RECORD_NOT_FOUND;
                }

                version.assign(msgc.dbmsg().version());
                rsp.Swap(msgc.mutable_dbmsg()->mutable_login());
                return hello::err::EN_SUCCESS;
            }

            int set(const char *openid, hello::table_login &rsp, std::string &version) {
                task_manager::task_t *task = task_manager::task_t::this_task();
                if (!task) {
                    WLOGERROR("current not in a task");
                    return hello::err::EN_SYS_RPC_NO_TASK;
                }

                if (version.empty()) {
                    version = "0";
                }

                DBLOGINKEY(user_key, writen_len, openid);
                if (writen_len <= 0) {
                    WLOGERROR("format db cmd failed, cmd %s", user_key);
                    return hello::err::EN_DB_SEND_FAILED;
                }

                std::vector<const ::google::protobuf::FieldDescriptor *> fds;
                const ::google::protobuf::Descriptor *desc = hello::table_login::descriptor();
                fds.reserve(static_cast<size_t>(desc->field_count()));

                for (int i = 0; i < desc->field_count(); ++i) {
                    fds.push_back(desc->field(i));
                }

                // version will take two segments
                // each fd will take key segment and value segment
                redis_args args(fds.size() * 2 + 6);
                {
                    args.push("EVALSHA");
                    args.push(db_msg_dispatcher::me()->get_db_script_sha1(hello::EN_DBSST_LOGIN));
                    args.push(1);
                    args.push(user_key);
                }

                std::stringstream segs_debug_info;
                int res = ::rpc::db::pack_message(rsp, args, fds, &version, &segs_debug_info);
                if (res < 0) {
                    return res;
                }

                res = db_msg_dispatcher::me()->send_msg(db_msg_dispatcher::channel_t::CLUSTER_DEFAULT, user_key, static_cast<size_t>(writen_len),
                                                        task->get_id(), logic_config::me()->get_self_bus_id(), detail::unpack_login,
                                                        static_cast<int>(args.size()), args.get_args_values(), args.get_args_lengths());

                // args unavailable now

                if (res < 0) {
                    return res;
                }

                hello::message_container msgc;
                // 协程操作
                res = rpc::wait(msgc);
                if (res < 0) {
                    if (hello::err::EN_DB_OLD_VERSION == res && msgc.has_dbmsg() && !msgc.dbmsg().version().empty()) {
                        version.assign(msgc.dbmsg().version());
                    }

                    return res;
                }

                if (!msgc.has_dbmsg() || msgc.dbmsg().version().empty()) {
                    return hello::err::EN_DB_RECORD_NOT_FOUND;
                }

                version.assign(msgc.dbmsg().version());

                WLOGINFO("table_login [openid=%s] all saved, new version: %s, detail: %s", openid, version.c_str(), segs_debug_info.str().c_str());
                return hello::err::EN_SUCCESS;
            }
        }
    }
}