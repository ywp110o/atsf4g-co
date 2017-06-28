//
// Created by owent on 2016/10/6.
//

#include <protocol/pbdesc/svr.const.err.pb.h>

#include <rpc/db/player.h>

#include <logic/session_manager.h>
#include <data/player.h>
#include <data/session.h>

#include "task_action_player_logout.h"


task_action_player_logout::task_action_player_logout(uint64_t bus_id, uint64_t session_id):
    atgateway_bus_id_(bus_id), atgateway_session_id_(session_id){}
task_action_player_logout::~task_action_player_logout() {}

int task_action_player_logout::operator()(hello::message_container& msg) {
    session::key_t key;
    key.bus_id = atgateway_bus_id_;
    key.session_id = atgateway_session_id_;
    session::ptr_t s = session_manager::me()->find(key);
    if (s) {
        // 连接断开的时候需要保存一下数据
        player::ptr_t user = s->get_player();
        // 如果玩家数据是缓存，不是实际登入点，则不用保存
        if (user && user->is_inited()) {

            hello::table_user user_tb;
            user->dump(user_tb, true);

            std::string db_version = user->get_version();
            int res = rpc::db::player::set(user->get_open_id().c_str(), user_tb, db_version);
            if (res >= 0 && !db_version.empty()) {
                user->set_version(db_version);
            }
        }
    }

    session_manager::me()->remove(key);
    return hello::err::EN_SUCCESS;
}

int task_action_player_logout::on_success() {
    return get_ret_code();
}

int task_action_player_logout::on_failed() {
    return get_ret_code();
}