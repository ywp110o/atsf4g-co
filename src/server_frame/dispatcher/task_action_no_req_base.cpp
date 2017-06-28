//
// Created by owt50 on 2016/9/26.
//

#include <protocol/pbdesc/svr.const.err.pb.h>

#include "task_action_no_req_base.h"

#include <data/player.h>


task_action_no_req_base::task_action_no_req_base() { }

task_action_no_req_base::~task_action_no_req_base() { }

std::shared_ptr<player> task_action_no_req_base::get_player() const {
    return player::get_global_gm_player();
}

void task_action_no_req_base::send_rsp_msg() {
}
