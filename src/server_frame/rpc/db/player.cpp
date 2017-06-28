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

#include "player.h"


namespace rpc {
    namespace db {
        namespace player {


            namespace detail {
                static int32_t unpack_user(hello::message_container &msgc, const redisReply* reply) {
                    if (NULL == reply) {
                        WLOGDEBUG("data mot found.");
                        // 数据找不到，直接成功结束，外层会判为无数据
                        return hello::err::EN_SUCCESS;
                    }

                    return ::rpc::db::unpack_message(*msgc.mutable_dbmsg()->mutable_user(), reply, msgc.mutable_dbmsg()->mutable_version());
                }
            }

            int get_all(const char* openid, hello::table_user& rsp, std::string &version) {
                task_manager::task_t* task = task_manager::task_t::this_task();
                if (!task) {
                    WLOGERROR("current not in a task");
                    return hello::err::EN_SYS_RPC_NO_TASK;
                }

                DBPLAYERKEY(user_key, user_key_len, openid);
                if (user_key_len <= 0) {
                    WLOGERROR("format db cmd failed, cmd %s", user_key);
                    return hello::err::EN_DB_SEND_FAILED;
                }

                int res = db_msg_dispatcher::me()->send_msg(
                    db_msg_dispatcher::channel_t::CLUSTER_DEFAULT,
                    user_key, user_key_len,
                    task->get_id(),
                    logic_config::me()->get_self_bus_id(),
                    detail::unpack_user,
                    DBPLAYERCMD(HGETALL, openid, "")
                );

                if (res < 0) {
                    return res;
                }

                hello::message_container msgc;
                // 协程操作
                res = rpc::wait(msgc);
                if (res < 0) {
                    return res;
                }

                if(!msgc.has_dbmsg() || !msgc.dbmsg().has_user() || msgc.dbmsg().version().empty()) {
                    return hello::err::EN_DB_RECORD_NOT_FOUND;
                }

                version.assign(msgc.dbmsg().version());
                rsp.Swap(msgc.mutable_dbmsg()->mutable_user());

                WLOGINFO("table_user[openid=%s] get all data version: %s",
                         openid,
                         version.c_str()
                );
                return hello::err::EN_SUCCESS;
            }

            int set(const char* openid, hello::table_user& store, std::string &version) {
                task_manager::task_t* task = task_manager::task_t::this_task();
                if (!task) {
                    WLOGERROR("current not in a task");
                    return hello::err::EN_SYS_RPC_NO_TASK;
                }

                if (version.empty()) {
                    version = "0";
                }

                std::stringstream segs_debug_info;

                DBPLAYERKEY(user_key, user_key_len, openid);
                if (user_key_len <= 0) {
                    WLOGERROR("format db cmd failed, cmd %s", user_key);
                    return hello::err::EN_DB_SEND_FAILED;
                }

                std::vector<const ::google::protobuf::FieldDescriptor*> fds;
                const google::protobuf::Reflection* reflect = store.GetReflection();
                if (NULL == reflect) {
                    WLOGERROR("pack message %s failed, get reflection failed", store.GetDescriptor()->full_name().c_str());
                    return hello::err::EN_SYS_PACK;
                }
                reflect->ListFields(store, &fds);
                // version will take two segments
                // each fd will take key segment and value segment
                redis_args args(fds.size() * 2 + 6);

                args.push("EVALSHA");
                args.push(db_msg_dispatcher::me()->get_db_script_sha1(hello::EN_DBSST_PLAYER));
                args.push(1);
                args.push(user_key);

                int res = ::rpc::db::pack_message(store, args, fds, &version, &segs_debug_info);
                if (res < 0) {
                    return res;
                }

                WLOGDEBUG("user %s save curr data version:%s", openid, version.c_str());

                res = db_msg_dispatcher::me()->send_msg(
                    db_msg_dispatcher::channel_t::CLUSTER_DEFAULT,
                    user_key, user_key_len,
                    task->get_id(),
                    logic_config::me()->get_self_bus_id(),
                    detail::unpack_user,
                    static_cast<int>(args.size()),
                    args.get_args_values(), args.get_args_lengths()
                );

                // args unavailable now

                if (res < 0) {
                    return res;
                }

                hello::message_container msgc;
                // 协程操作
                res = rpc::wait(msgc);
                if (res < 0) {
                    if (hello::err::EN_DB_OLD_VERSION == res && msgc.has_dbmsg()
                        && !msgc.dbmsg().version().empty()) {
                        version.assign(msgc.dbmsg().version());
                    }
                    return res;
                }

                if(!msgc.has_dbmsg() || msgc.dbmsg().version().empty()) {
                    return hello::err::EN_DB_RECORD_NOT_FOUND;
                }

                version.assign(msgc.dbmsg().version());

                WLOGINFO("table_user [openid=%s] all saved, new version: %s, detail: %s",
                         openid,
                         version.c_str(),
                         segs_debug_info.str().c_str()
                );

                return hello::err::EN_SUCCESS;
            }

        }
    }
}