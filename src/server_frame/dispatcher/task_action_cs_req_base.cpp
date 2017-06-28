//
// Created by owt50 on 2016/9/26.
//

#include "task_action_cs_req_base.h"

#include <data/player.h>
#include <log/log_wrapper.h>
#include <logic/session_manager.h>
#include <time/time_utility.h>


task_action_cs_req_base::task_action_cs_req_base() {}
task_action_cs_req_base::~task_action_cs_req_base() {}

task_action_cs_req_base::msg_cptr_type task_action_cs_req_base::get_cs_request() const {
    const base_type::msg_type &msgc = get_request();
    if (!msgc.has_csmsg()) {
        return NULL;
    }

    return &msgc.csmsg();
}

task_action_cs_req_base::msg_ptr_type task_action_cs_req_base::get_cs_request() {
    base_type::msg_type &msgc = get_request();
    return msgc.mutable_csmsg();
}

std::pair<uint64_t, uint64_t> task_action_cs_req_base::get_gateway_info() const {
    const base_type::msg_type &msgc = get_request();
    if (!msgc.has_src_client() || !msgc.has_src_server()) {
        return std::pair<uint64_t, uint64_t>(0, 0);
    }

    return std::pair<uint64_t, uint64_t>(msgc.src_server().bus_id(), msgc.src_client().session_id());
};

std::shared_ptr<session> task_action_cs_req_base::get_session() const {
    if (session_inst_) {
        return session_inst_;
    }

    session::key_t key(get_gateway_info());
    session_inst_ = session_manager::me()->find(key);
    return session_inst_;
}

std::shared_ptr<player> task_action_cs_req_base::get_player() const {
    session::ptr_t sess = get_session();

    if (sess) {
        return sess->get_player();
    }

    return NULL;
}

task_action_cs_req_base::msg_ref_type task_action_cs_req_base::add_rsp_msg() {
    rsp_msgs_.push_back(msg_type());
    return rsp_msgs_.back();
}

std::list<task_action_cs_req_base::msg_type> &task_action_cs_req_base::get_rsp_list() { return rsp_msgs_; }

const std::list<task_action_cs_req_base::msg_type> &task_action_cs_req_base::get_rsp_list() const { return rsp_msgs_; }

void task_action_cs_req_base::send_rsp_msg() {
    if (rsp_msgs_.empty()) {
        return;
    }

    session::ptr_t sess = get_session();
    if (!sess) {
        player::ptr_t owner_player = get_player();
        if (owner_player) {
            WLOGERROR("try to send response message to player %s, but session not found", owner_player->get_open_id().c_str());
        } else {
            WLOGERROR("try to send response message, but session and player not found");
        }
        return;
    }

    uint32_t seq = 0;
    {
        msg_cptr_type req_msg = get_cs_request();
        if (NULL != req_msg && req_msg->has_head()) {
            seq = req_msg->head().sequence();
        }
    }

    for (std::list<msg_type>::iterator iter = rsp_msgs_.begin(); iter != rsp_msgs_.end(); ++iter) {
        (*iter).mutable_head()->set_error_code(get_rsp_code());
        (*iter).mutable_head()->set_timestamp(util::time::time_utility::get_now());
        (*iter).mutable_head()->set_sequence(seq);

        // send message using session
        int32_t res = sess->send_msg_to_client(*iter);
        if (res) {
            player::ptr_t owner_player = get_player();
            if (owner_player) {
                WLOGERROR("task %s [0x%llx] send message to player %s failed, res: %d", name(), get_task_id_llu(), owner_player->get_open_id().c_str(), res);
            } else {
                WLOGERROR("task %s [0x%llx] send message to session [0x%llx,0x%llx] failed, res: %d", name(), get_task_id_llu(),
                          static_cast<unsigned long long>(sess->get_key().bus_id), static_cast<unsigned long long>(sess->get_key().session_id), res);
            }
        }
    }

    rsp_msgs_.clear();
}