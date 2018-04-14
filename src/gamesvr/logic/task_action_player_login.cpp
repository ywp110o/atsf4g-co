//
// Created by owent on 2016/10/6.
//

#include <common/string_oprs.h>
#include <log/log_wrapper.h>
#include <protocol/pbdesc/svr.const.err.pb.h>

#include <rpc/db/login.h>

#include <data/player.h>
#include <data/session.h>
#include <logic/player_manager.h>
#include <logic/session_manager.h>


#include <config/logic_config.h>
#include <proto_base.h>
#include <rpc/db/player.h>
#include <time/time_utility.h>


#include "task_action_player_login.h"


task_action_player_login::task_action_player_login(dispatcher_start_data_t COPP_MACRO_RV_REF param)
    : task_action_cs_req_base(COPP_MACRO_STD_MOVE(param)), is_new_player_(false) {}
task_action_player_login::~task_action_player_login() {}

int task_action_player_login::operator()() {
    is_new_player_ = false;

    // 1. 包校验
    msg_cref_type req = get_request();
    if (!req.has_body() || !req.body().has_mcs_login_req()) {
        WLOGERROR("login package error, msg: %s", req.DebugString().c_str());
        set_rsp_code(hello::EN_ERR_INVALID_PARAM);
        return hello::err::EN_SUCCESS;
    }

    int res = 0;
    const ::hello::CSLoginReq &msg_body = req.body().mcs_login_req();

    // 先查找用户缓存，使用缓存。如果缓存正确则不需要拉取login表和user表
    player::ptr_t user = player_manager::me()->find(msg_body.user_id());
    if (user && user->get_login_info().login_code() == msg_body.login_code() &&
        util::time::time_utility::get_now() <= static_cast<time_t>(user->get_login_info().login_code_expired()) && user->is_inited()) {
        WLOGDEBUG("player %s(%llu) relogin using login code", user->get_open_id().c_str(), user->get_user_id_llu());

        // 获取当前Session
        std::shared_ptr<session> cur_sess = get_session();
        if (!cur_sess) {
            WLOGERROR("session not found");
            return hello::err::EN_SUCCESS;
        }

        // 踢出前一个session
        std::shared_ptr<session> old_sess = user->get_session();

        // 重复的登入包直接接受
        if (cur_sess == old_sess) {
            return hello::err::EN_SUCCESS;
        }

        user->set_session(cur_sess);
        if (old_sess) {
            // 下发踢下线包，防止循环重连互踢
            old_sess->set_player(NULL);
            session_manager::me()->remove(old_sess, ::atframe::gateway::close_reason_t::EN_CRT_KICKOFF);
        }
        cur_sess->set_player(user);

        // TODO login success and try to restore daily limit
        // TODO 尝试重新走未完成的支付订单的流程
        // hello::SCItemChgSyn itemchg;
        // user->GetRechargeMgr().ApplyOrder(itemchg);

        WLOGDEBUG("player %s(%llu) relogin curr data version:%s", user->get_open_id().c_str(), user->get_user_id_llu(), user->get_version().c_str());

        return hello::err::EN_SUCCESS;
    }

    hello::table_login tb;
    std::string version;
    res = rpc::db::login::get(msg_body.open_id().c_str(), tb, version);
    if (res < 0) {
        WLOGERROR("player %s not found", msg_body.open_id().c_str());
        set_rsp_code(hello::EN_ERR_INVALID_PARAM);
        return hello::err::EN_SUCCESS;
    }

    if (msg_body.user_id() != tb.user_id()) {
        WLOGERROR("player %s expect user_id=%llu, but we got %llu not found", msg_body.open_id().c_str(), static_cast<unsigned long long>(tb.user_id()),
                  static_cast<unsigned long long>(msg_body.user_id()));
        set_rsp_code(hello::EN_ERR_LOGIN_USERID_NOT_MATCH);
        return hello::err::EN_SUCCESS;
    }

    // 2. 校验登入码
    if (util::time::time_utility::get_now() > tb.login_code_expired()) {
        WLOGERROR("player %s(%llu) login code expired", msg_body.open_id().c_str(), static_cast<unsigned long long>(msg_body.user_id()));
        set_rsp_code(hello::EN_ERR_LOGIN_VERIFY);
        return hello::err::EN_SUCCESS;
    }

    if (0 != UTIL_STRFUNC_STRCMP(msg_body.login_code().c_str(), tb.login_code().c_str())) {
        WLOGERROR("player %s(%llu) login code error(expected: %s, real: %s)", msg_body.open_id().c_str(), static_cast<unsigned long long>(msg_body.user_id()),
                  tb.login_code().c_str(), msg_body.login_code().c_str());
        set_rsp_code(hello::EN_ERR_LOGIN_VERIFY);
        return hello::err::EN_SUCCESS;
    }

    // 3. 写入登入信息
    tb.set_login_pd(logic_config::me()->get_self_bus_id());
    tb.set_login_time(util::time::time_utility::get_now());

    res = rpc::db::login::set(msg_body.open_id().c_str(), tb, version);
    if (res < 0) {
        WLOGERROR("save login data for %s(%llu) failed, msg:\n%s", msg_body.open_id().c_str(), static_cast<unsigned long long>(msg_body.user_id()),
                  tb.DebugString().c_str());
        set_rsp_code(hello::EN_ERR_SYSTEM);
        return res;
    }

    // 4. 先读本地缓存
    std::shared_ptr<session> my_sess = get_session();
    if (!my_sess) {
        WLOGERROR("session not found");
    }

    if (!user || !user->is_inited()) {
        if (!user) {
            user = player_manager::me()->create(msg_body.user_id(), msg_body.open_id());
        }

        hello::table_user tbu;
        // 5. 先尝试从数据库读数据
        res = rpc::db::player::get_all(msg_body.user_id(), tbu, user->get_version());
        if (hello::err::EN_DB_RECORD_NOT_FOUND != res && res < 0) {
            WLOGERROR("load player data for %s(%llu) failed, error code:%d", user->get_open_id().c_str(), user->get_user_id_llu(), res);
            set_rsp_code(hello::EN_ERR_SYSTEM);

            return res;
        }

        if (hello::err::EN_DB_RECORD_NOT_FOUND == res) {
            // 生成name到uuid的映射
            std::string nick_name = user->get_open_id() + " - Nickname"; // TODO random a name
            // TODO 如果需要分配并设置数字UUID、随机nickname和建立名字和账号的对应关系， do it here, 注意重名的重试

            user->get_platform_info().set_platform_id(tb.platform().platform_id());
            user->get_platform_info().set_zone_id(tb.platform().zone_id());
            user->get_platform_info().set_channel_id(tb.platform().channel_id());
            user->get_platform_info().set_access(tb.platform().access());
            user->get_platform_info().set_version_type(tb.platform().version_type());
            user->get_platform_info().mutable_profile()->set_nick_name(nick_name);

            // 6. 没有数据的话就创建新用户
            user->create_init(tb.platform().version_type());

            // 7. 然后保存一次数据库
            tbu.Clear();
            user->dump(tbu, true);

            res = rpc::db::player::set(user->get_user_id(), tbu, user->get_version());
            if (res < 0) {
                WLOGERROR("save player data for %s(%llu) failed, msg:\n%s", user->get_open_id().c_str(), user->get_user_id_llu(), tbu.DebugString().c_str());
                set_rsp_code(hello::EN_ERR_SYSTEM);
                return res;
            }

            is_new_player_ = true;
        } else {
            user->init_from_table_data(tbu);
        }
    }

    user->get_platform_info().set_access(tb.platform().access());
    user->get_platform_info().set_platform_id(tb.platform().platform_id());
    user->get_platform_info().set_version_type(tb.platform().version_type());
    user->set_client_info(req.body().mcs_login_req().client_info());

    // 保存登入信息 一定在loginit前面 有些模块会用到loginfo信息
    user->get_login_info().Swap(&tb);

    // 8. 设置和Session互相关联
    user->set_session(my_sess);
    user->login_init();

    // 如果不存在则是登入过程中掉线了
    if (!my_sess) {
        set_rsp_code(hello::EN_ERR_NOT_LOGIN);
        return hello::err::EN_SUCCESS;
    }

    my_sess->set_player(user);

    // TODO login success and try to restore daily limit
    // TODO 尝试重新走未完成的支付订单的流程
    WLOGDEBUG("player %s(%llu) login curr data version:%s", user->get_open_id().c_str(), user->get_user_id_llu(), user->get_version().c_str());

    // 9. 登入成功
    return hello::err::EN_SUCCESS;
}

int task_action_player_login::on_success() {
    hello::CSMsg &msg = add_rsp_msg();
    ::hello::SCLoginRsp *rsp_body = msg.mutable_body()->mutable_msc_login_rsp();

    rsp_body->set_heartbeat_interval(static_cast<uint32_t>(logic_config::me()->get_cfg_logic().heartbeat_interval));
    rsp_body->set_is_new_player(is_new_player_);

    std::shared_ptr<session> s = get_session();
    if (s) {
        s->set_login_task_id(0);
    }

    // 1. 包校验
    msg_cref_type req = get_request();
    if (!req.has_body() || !req.body().has_mcs_login_req()) {
        WLOGERROR("login package error, msg: %s", req.DebugString().c_str());
        return get_ret_code();
    }

    player::ptr_t user = player_manager::me()->find(req.body().mcs_login_req().user_id());
    if (!user) {
        WLOGERROR("login success but user not found");
        return get_ret_code();
    }
    rsp_body->set_zone_id(user->get_zone_id());

    rsp_body->set_version_type(user->get_platform_info().version_type());

    // 设置/覆盖定时保存
    player_manager::me()->update_auto_save(user);

    // TODO 断线重连，上次收包序号
    // rsp_body->set_last_sequence(user->get_cache_data());

    if (!user->is_inited()) {
        WLOGERROR("player %s login success but user not inited", user->get_open_id().c_str());
        player_manager::me()->remove(user, true);
        return get_ret_code();
    }

    return get_ret_code();
}

int task_action_player_login::on_failed() {
    // 1. 包校验
    msg_cref_type req = get_request();

    if (req.has_body() && req.body().has_mcs_login_req()) {
        player::ptr_t user = player_manager::me()->find(req.body().mcs_login_req().user_id());
        // 如果创建了未初始化的GameUser对象，则需要移除
        if (user && !user->is_inited()) {
            player_manager::me()->remove(user, true);
        }
    }

    // 登入过程中掉线了，直接退出
    std::shared_ptr<session> s = get_session();
    if (!s) {
        WLOGERROR("session [0x%llx,0x%llx] not found", static_cast<unsigned long long>(get_gateway_info().first),
                  static_cast<unsigned long long>(get_gateway_info().second));
        return hello::err::EN_SUCCESS;
    }

    WLOGERROR("session [0x%llx,0x%llx] login failed, rsp code: %d, ret code: %d", static_cast<unsigned long long>(get_gateway_info().first),
              static_cast<unsigned long long>(get_gateway_info().second), get_rsp_code(), get_ret_code());

    hello::CSMsg &msg = add_rsp_msg();

    ::hello::SCLoginRsp *rsp_body = msg.mutable_body()->mutable_msc_login_rsp();
    rsp_body->set_last_sequence(0);
    rsp_body->set_zone_id(0);

    // 手动发包并无情地踢下线
    send_rsp_msg();

    session_manager::me()->remove(s, ::atframe::gateway::close_reason_t::EN_CRT_FIRST_IDLE);
    return get_ret_code();
}