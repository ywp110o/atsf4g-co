//
// Created by owent on 2016/10/6.
//

#include <log/log_wrapper.h>
#include <protocol/pbdesc/svr.const.err.pb.h>
#include <protocol/pbdesc/svr.container.pb.h>

#include <rpc/db/login.h>

#include <logic/player_manager.h>
#include <logic/session_manager.h>
#include <data/player.h>
#include <data/session.h>

#include <config/logic_config.h>

#include <config/extern_service_types.h>
#include <proto_base.h>

#include "task_action_player_kickoff.h"

task_action_player_kickoff::task_action_player_kickoff() {}
task_action_player_kickoff::~task_action_player_kickoff() {}

int task_action_player_kickoff::operator()(hello::message_container& msg) {
    player::ptr_t user = get_player();
    if(!user) {
        WLOGERROR("user %s not found, maybe already logout.", get_player_openid().c_str());

        // 尝试保存用户数据
        hello::table_login user_lg;
        std::string version;
        int res = rpc::db::login::get(get_player_openid().c_str(), user_lg, version);
        if (res < 0) {
            WLOGERROR("user %s try load login data failed.", get_player_openid().c_str());
            set_rsp_code(hello::err::EN_DB_REPLY_ERROR);
            return hello::err::EN_SUCCESS;
        }

        if (user_lg.login_pd() != logic_config::me()->get_self_bus_id()) {
            WLOGERROR("user %s login pd error(expected: 0x%llx, real: 0x%llx)",
                      get_player_openid().c_str(),
                      static_cast<unsigned long long>(user_lg.login_pd()),
                      static_cast<unsigned long long>(logic_config::me()->get_self_bus_id())
            );
            set_rsp_code(hello::EN_ERR_SYSTEM);
            return hello::err::EN_SUCCESS;
        }

        user_lg.set_login_pd(0);
        res = rpc::db::login::set(get_player_openid().c_str(), user_lg, version);
        if (res < 0) {
            WLOGERROR("user %s try load login data failed.", get_player_openid().c_str());
            set_rsp_code(hello::err::EN_DB_SEND_FAILED);
            return hello::err::EN_SUCCESS;
        }

        return hello::err::EN_SUCCESS;
    }

    // 仅在有session时才下发踢出消息
    std::shared_ptr<session> sess = user->get_session();
    if (sess) {
        int32_t ret = sess->send_kickoff(::atframe::gateway::close_reason_t::EN_CRT_KICKOFF);
        if (ret) {
            WLOGERROR("task %s [0x%llx] send cs msg failed, ret: %d\n",
                      name(), get_task_id_llu(), ret);

            // 发送失败也没有关系，下次客户端发包的时候自然会出错
        }
    }

    if(!player_manager::me()->remove(user, true)) {
        WLOGERROR("kickoff user %s failed", user->get_open_id().c_str());
        set_rsp_code(hello::EN_ERR_SYSTEM);
        return hello::err::EN_SUCCESS;
    }

    return hello::err::EN_SUCCESS;
}

int task_action_player_kickoff::on_success() {
    hello::message_container &rsp = add_rsp_msg(::atframe::component::message_type::EN_ATST_SS_MSG);
    rsp.mutable_ssmsg()->mutable_body()->mutable_mss_player_kickoff_rsp();

    return get_ret_code();
}

int task_action_player_kickoff::on_failed() {
    hello::message_container &rsp = add_rsp_msg(::atframe::component::message_type::EN_ATST_SS_MSG);
    rsp.mutable_ssmsg()->mutable_body()->mutable_mss_player_kickoff_rsp();

    return get_ret_code();
}