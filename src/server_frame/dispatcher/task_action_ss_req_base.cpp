//
// Created by owt50 on 2016/9/26.
//

#include <log/log_wrapper.h>
#include <logic/player_manager.h>
#include <time/time_utility.h>


#include <dispatcher/ss_msg_dispatcher.h>

#include "task_action_ss_req_base.h"

task_action_ss_req_base::task_action_ss_req_base(dispatcher_start_data_t COPP_MACRO_RV_REF start_param) {
    msg_type *ss_msg = ss_msg_dispatcher::me()->get_protobuf_msg<msg_type>(start_param.message);
    if (NULL != ss_msg) {
        get_request().Swap(ss_msg);

        set_player_id(get_request().head().player_user_id());
    }
}

task_action_ss_req_base::~task_action_ss_req_base() {}

int task_action_ss_req_base::hook_run() {
    // TODO 路由对象系统支持
    if (get_request().head().has_router()) {
        do {
            // TODO find router manager in router set
            // TODO 如果正在迁移，追加到pending队列，本task直接退出
            // TODO 如果和本地的路由缓存匹配则break直接开始消息处理
            // TODO 如果本地版本号低于来源服务器，刷新一次路由表

            // TODO mutable路由对象，检查是否成功

            // TODO 如果本地路由版本号大于来源，通知来源更新路由表

            // TODO 如果和本地的路由缓存匹配则break直接开始消息处理

            // TODO 路由消息转发
            // TODO 如果路由转发成功，需要禁用掉回包和通知事件
            disable_rsp_msg();
            disable_finish_evt();

            // TODO 如果路由对象不在任何节点上，返回错误
            set_rsp_code(hello::err::EN_ROUTER_NOT_IN_SERVER);
        } while (false);
    }

    return base_type::hook_run();
}

uint64_t task_action_ss_req_base::get_request_bus_id() const {
    msg_cref_type msg = get_request();
    return msg.head().bus_id();
}

task_action_ss_req_base::msg_ref_type task_action_ss_req_base::add_rsp_msg(uint64_t dst_pd) {
    rsp_msgs_.push_back(msg_type());
    msg_ref_type msg = rsp_msgs_.back();

    msg.mutable_head()->set_error_code(get_rsp_code());
    dst_pd = 0 == dst_pd ? get_request_bus_id() : dst_pd;

    init_msg(msg, dst_pd, get_request());
    return msg;
}


int32_t task_action_ss_req_base::init_msg(msg_ref_type msg, uint64_t dst_pd) {
    msg.mutable_head()->set_bus_id(dst_pd);
    msg.mutable_head()->set_timestamp(util::time::time_utility::get_now());

    return 0;
}

int32_t task_action_ss_req_base::init_msg(msg_ref_type msg, uint64_t dst_pd, msg_cref_type req_msg) {
    msg.mutable_head()->CopyFrom(req_msg.head());
    init_msg(msg, dst_pd);

    // set task information
    if (0 != req_msg.head().src_task_id()) {
        msg.mutable_head()->set_dst_task_id(req_msg.head().src_task_id());
    } else {
        msg.mutable_head()->set_dst_task_id(0);
    }

    if (0 != req_msg.head().dst_task_id()) {
        msg.mutable_head()->set_src_task_id(req_msg.head().dst_task_id());
    } else {
        msg.mutable_head()->set_src_task_id(0);
    }

    return 0;
}

void task_action_ss_req_base::send_rsp_msg() {
    if (rsp_msgs_.empty()) {
        return;
    }

    for (std::list<msg_type>::iterator iter = rsp_msgs_.begin(); iter != rsp_msgs_.end(); ++iter) {
        if (0 == (*iter).head().bus_id()) {
            WLOGERROR("task %s [0x%llx] send message to unknown server", name(), get_task_id_llu());
            continue;
        }
        (*iter).mutable_head()->set_error_code(get_rsp_code());

        // send message using ss dispatcher
        int32_t res = ss_msg_dispatcher::me()->send_to_proc((*iter).head().bus_id(), *iter);
        if (res) {
            WLOGERROR("task %s [0x%llx] send message to server 0x%llx failed, res: %d", name(), get_task_id_llu(),
                      static_cast<unsigned long long>((*iter).head().bus_id()), res);
        }
    }

    rsp_msgs_.clear();
}