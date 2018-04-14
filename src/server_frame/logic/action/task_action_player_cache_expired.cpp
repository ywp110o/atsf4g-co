//
// Created by owent on 2016/10/6.
//

#include <fstream>
#include <log/log_wrapper.h>
#include <protocol/pbdesc/svr.const.err.pb.h>
#include <rpc/db/player.h>

#include <data/player.h>
#include <data/session.h>
#include <logic/player_manager.h>
#include <logic/session_manager.h>
#include <time/time_utility.h>

#include <config/logic_config.h>
#include <proto_base.h>

#include "task_action_player_cache_expired.h"

task_action_player_cache_expired::task_action_player_cache_expired(ctor_param_t COPP_MACRO_RV_REF param)
    : success_count_(0), failed_count_(0), ctor_param_(COPP_MACRO_STD_MOVE(param)) {}

task_action_player_cache_expired::~task_action_player_cache_expired() {}

int task_action_player_cache_expired::operator()() {
    success_count_ = failed_count_ = 0;

    std::list<player_manager::player_cache_t> &cache_list = player_manager::me()->get_cache_expire_list();

    time_t now_tm = util::time::time_utility::get_now();

    while (!cache_list.empty()) {
        player_manager::player_cache_t cache = cache_list.front();
        player::ptr_t user = cache.player_inst.lock();

        // 如果已下线并且用户缓存失效则跳过
        if (!user) {
            cache_list.pop_front();
            continue;
        }

        // 如果操作序列失效则跳过
        if (false == user->check_logout_cache(cache.operation_sequence)) {
            cache_list.pop_front();
            continue;
        }

        // 不需要保存则跳过, 不保存的话不用管时间了
        if (false == cache.save) {
            // 释放用户索引
            player_manager::me()->force_erase(user->get_user_id());

            cache_list.pop_front();
            continue;
        }

        // 如果没到时间，后面的全没到时间
        if (now_tm <= cache.expire_time) {
            break;
        }

        // 先pop，否则后续的异步处理还可能触发缓存过期
        cache_list.pop_front();

        if (player_manager::me()->remove(user, true)) {
            ++success_count_;
            WLOGDEBUG("save player cache %s(%llu) success", user->get_open_id().c_str(), user->get_user_id_llu());
        } else {
            ++failed_count_;

            // 失败重试
            bool store_to_disk = false;
            if (cache.failed_times <= logic_config::me()->get_cfg_logic().player_cache_max_retry_times &&
                logic_config::me()->get_cfg_logic().player_cache_expire_time > 0) {
                player_manager::player_cache_t *pcache = player_manager::me()->set_offline_cache(user);
                if (NULL != pcache) {
                    pcache->failed_times = cache.failed_times + 1;
                    WLOGWARNING("save player cache %s(%llu) failed, and will try again later", user->get_open_id().c_str(), user->get_user_id_llu());
                } else {
                    store_to_disk = true;
                    WLOGERROR("retry player user cache %s(%llu) failed", user->get_open_id().c_str(), user->get_user_id_llu());
                }
            } else {
                store_to_disk = true;
            }

            if (store_to_disk) {
                // 超出失败次数，强制踢出，缓存用户数据
                WLOGERROR("save player cache %s(%llu) failed", user->get_open_id().c_str(), user->get_user_id_llu());
                hello::table_user db;
                user->dump(db, true);

                std::string file_path = logic_config::me()->get_cfg_logic().server_resource_dir + "/cache/" + user->get_open_id() + ".data.bin";
                std::fstream dump_file;
                dump_file.open(file_path.c_str(), std::ios::out | std::ios::binary);
                if (dump_file.is_open()) {
                    db.SerializePartialToOstream(&dump_file);
                } else {
                    WLOGERROR("open file %s to dump user data failed.", file_path.c_str());
                }

                // 强制移除
                session::ptr_t s = user->get_session();
                if (s) {
                    user->set_session(NULL);
                    s->set_player(NULL);
                    session_manager::me()->remove(s, ::atframe::gateway::close_reason_t::EN_CRT_KICKOFF);
                }
                player_manager::me()->force_erase(user->get_user_id());
            }

            // 如果出错，可能是超时，本次直接退出，下次重启Task继续
            break;
        }
    }

    return hello::err::EN_SUCCESS;
}

int task_action_player_cache_expired::on_success() {
    WLOGINFO("clear player cache task done.(success save: %d, failed save: %d)", success_count_, failed_count_);

    if (0 == success_count_ && 0 == failed_count_) {
        WLOGWARNING("there is no need to start a clear player cache task when no user expired.");
    }
    return get_ret_code();
}

int task_action_player_cache_expired::on_failed() {
    WLOGERROR("clear player cache task failed.(success save: %d, failed save: %d) ret: %d", success_count_, failed_count_, get_ret_code());
    return get_ret_code();
}

int task_action_player_cache_expired::on_timeout() {
    WLOGWARNING("clear player cache task timeout, we will continue on next round.");
    return 0;
}