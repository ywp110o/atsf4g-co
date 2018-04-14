//
// Created by owent on 2016/10/6.
//

#include <log/log_wrapper.h>
#include <protocol/pbdesc/svr.const.err.pb.h>
#include <rpc/db/login.h>
#include <rpc/db/player.h>


#include <data/player.h>
#include <data/session.h>
#include <logic/player_manager.h>
#include <time/time_utility.h>

#include <config/logic_config.h>

#include "task_action_auto_save_players.h"


task_action_auto_save_players::task_action_auto_save_players(ctor_param_t COPP_MACRO_RV_REF param)
    : success_count_(0), failed_count_(0), ctor_param_(COPP_MACRO_STD_MOVE(param)) {}

task_action_auto_save_players::~task_action_auto_save_players() {}

int task_action_auto_save_players::operator()() {
    success_count_ = failed_count_ = 0;

    std::list<std::weak_ptr<player> > &save_list = player_manager::me()->get_auto_save_list();

    size_t limit_count = logic_config::me()->get_cfg_logic().player_auto_save_limit;

    time_t now_tm = util::time::time_utility::get_now();

    while (limit_count > 0) {

        if (save_list.empty()) {
            break;
        }

        player::ptr_t user = save_list.front().lock();

        // 如果已下线并且用户缓存失效则跳过
        if (!user) {
            save_list.pop_front();
            continue;
        }

        // 如果没有开启自动保存则跳过
        if (0 == user->get_schedule_data().save_pending_time) {
            save_list.pop_front();
            continue;
        }

        // 如果最近自动保存用户的保存时间大于当前时间，则没有用户需要保存数据
        if (now_tm <= user->get_schedule_data().save_pending_time) {
            break;
        }

        // 先pop，否则后续的异步处理还可能触发保存
        save_list.pop_front();

        int res = 0;
        hello::table_login login_tb;
        std::string login_ver;

        if (false == user->is_removing()) {
            hello::table_user user_data;

            // TODO 先强制转储全部数据，以后看需要优化成转储脏数据.
            user->dump(user_data, true);
            std::string user_version = user->get_version();
            res = rpc::db::player::set(user->get_user_id(), user_data, user_version);
            // 如果是序号错误,要能够自动修正
            // 定时保存仅当玩家在线才做修正，防止其他地方登入以后此处在缓存失效前触发错误
            if (hello::err::EN_DB_OLD_VERSION == res && user->get_session()) {
                res = player_manager::me()->save(user, &login_tb, &login_ver);
            }

            if (hello::err::EN_SUCCESS != res) {
                WLOGERROR("auto save user %s(%llu) failed, ret: %d", user->get_open_id().c_str(), user->get_user_id_llu(), res);
                ++failed_count_;
            } else {
                if (!user_version.empty()) {
                    user->set_version(user_version);
                }
                WLOGDEBUG("auto save user %s(%llu) success, version = %s", user->get_open_id().c_str(), user->get_user_id_llu(), user_version.c_str());
                ++success_count_;
            }

            // 如果有session，更新login表
            if (user->get_session()) {
                if (login_ver.empty()) {
                    res = rpc::db::login::get(user->get_open_id().c_str(), login_tb, login_ver);
                }
                if (res >= 0) {
                    if (logic_config::me()->get_self_bus_id() == login_tb.login_pd()) {
                        login_tb.set_login_code_expired(util::time::time_utility::get_now() + logic_config::me()->get_cfg_logic().session_login_code_valid_sec);
                        login_tb.set_login_code(user->get_login_info().login_code());

                        res = rpc::db::login::set(user->get_open_id().c_str(), login_tb, login_ver);
                        if (res < 0) {
                            WLOGERROR("call login rpc method set failed, res: %d, msg: %s", res, login_tb.DebugString().c_str());
                        }
                    }
                } else {
                    WLOGERROR("call login rpc method get failed, msg: %s", login_tb.DebugString().c_str());
                }

            } else {
                user->get_login_info().set_login_pd(0);
            }
        } else {
            WLOGDEBUG("auto save user %s when removing, skip and success", user->get_open_id().c_str());
            ++success_count_;
        }

        // 重置下一次自动保存信息
        user->reset_auto_save();
        player_manager::me()->update_auto_save(user);

        // 如果这个Task执行事件过长则换一个Task，本次Task结束
        if (hello::err::EN_SYS_TIMEOUT == res) {
            limit_count = 0;
        } else {
            --limit_count;
        }
    }

    return hello::err::EN_SUCCESS;
}

int task_action_auto_save_players::on_success() {
    WLOGINFO("auto save task done.(success save: %d, failed save: %d)", success_count_, failed_count_);

    if (0 == success_count_ && 0 == failed_count_) {
        WLOGWARNING("there is no need to start a auto save task when no user need save.");
    }
    return get_ret_code();
}

int task_action_auto_save_players::on_failed() {
    WLOGERROR("auto save task failed.(success save: %d, failed save: %d) ret: %d", success_count_, failed_count_, get_ret_code());
    return get_ret_code();
}

int task_action_auto_save_players::on_timeout() {
    WLOGWARNING("auto save task timeout, we will continue on next round.");
    return 0;
}