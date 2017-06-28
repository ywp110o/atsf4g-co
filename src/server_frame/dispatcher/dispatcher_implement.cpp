//
// Created by owt50 on 2016/9/26.
//

#include "dispatcher_implement.h"
#include "actor_action_base.h"

#include <common/string_oprs.h>
#include <log/log_wrapper.h>
#include <protocol/pbdesc/svr.const.err.pb.h>
#include <protocol/pbdesc/svr.container.pb.h>

#include <utility/protobuf_mini_dumper.h>

int dispatcher_implement::init() { return 0; }

int32_t dispatcher_implement::on_recv_msg(msg_ptr_t msgc, const void *msg_buf, size_t msg_size) {
    if (NULL == msgc) {
        return hello::err::EN_SYS_PARAM;
    }

    int32_t ret = unpack_msg(msgc, msg_buf, msg_size);
    if (ret < 0) {
        msgc->mutable_src_server()->set_rpc_result(ret);
        WLOGERROR("unpack message failed.");
    }

    if (NULL != WDTLOGGETCAT(util::log::log_wrapper::categorize_t::DEFAULT) &&
        WDTLOGGETCAT(util::log::log_wrapper::categorize_t::DEFAULT)->check(util::log::log_wrapper::level_t::LOG_LW_DEBUG)) {
        WLOGDEBUG("dispatcher %s recv msg.\n%s", name(), protobuf_mini_dumper_get_readable(*msgc));
    }

    // 消息过滤器
    // 用于提供给所有消息进行前置处理的功能
    // 过滤器可以控制消息是否要下发下去
    if (!msg_filter_list_.empty()) {
        msg_filter_data_t filter_data;
        filter_data.msg_buffer = msg_buf;
        filter_data.msg_size = msg_size;
        filter_data.msg = msgc;

        for (std::list<msg_filter_handle_t>::iterator iter = msg_filter_list_.begin(); iter != msg_filter_list_.end(); ++iter) {
            if (false == (*iter)(filter_data)) {
                return ret;
            }
        }
    }

    uint64_t task_id = pick_msg_task(msgc);
    if (task_id > 0) { // 如果是恢复任务则尝试切回协程任务
        // 查找并恢复已有task
        return task_manager::me()->resume_task(task_id, *msgc);
    } else if (ret >= 0) {
        // 先尝试使用actor 模块
        task_manager::actor_action_ptr_t actor;
        if (!actor_action_name_map_.empty()) {
            actor = create_actor(msgc);
        }

        int res = create_task(msgc, task_id);

        // 仅actor
        if (actor && hello::err::EN_SYS_NOTFOUND == res) {
            return actor->run(msgc);
        }

        if (actor) {
            // 复制消息，隔离task和actor。防止互相影响。
            hello::message_container msg_copied;
            msg_copied.CopyFrom(*msgc);
            actor->run(&msg_copied);
        }

        // dst_id
        msgc->mutable_src_server()->set_dst_task_id(task_id);

        if (res < 0) {
            WLOGERROR("create task failed, errcode=%d\n%s", res, protobuf_mini_dumper_get_readable(*msgc));
            return hello::err::EN_SYS_MALLOC;
        }

        // 不创建消息
        if (res == 0 && 0 == task_id) {
            return hello::err::EN_SUCCESS;
        }

        // 再启动
        return task_manager::me()->start_task(task_id, *msgc);
    }

    return ret;
}

uint64_t dispatcher_implement::pick_msg_task(const msg_ptr_t msg_container) {
    if (NULL == msg_container || !msg_container->has_src_server()) {
        return 0;
    }

    return msg_container->src_server().dst_task_id();
}

int dispatcher_implement::create_task(const msg_ptr_t msg_container, task_manager::id_t &task_id) {
    task_id = 0;
    if (NULL == msg_container) {
        return hello::err::EN_SYS_PARAM;
    }

    msg_type_t msg_type_id = pick_msg_type_id(msg_container);
    if (0 == msg_type_id) {
        return hello::err::EN_SUCCESS;
    }

    if (task_action_name_map_.empty()) {
        return hello::err::EN_SYS_PARAM;
    }

    msg_task_action_set_t::iterator iter = task_action_name_map_.find(msg_type_id);
    if (task_action_name_map_.end() == iter) {
        return hello::err::EN_SYS_NOTFOUND;
    }

    return iter->second(task_id);
}

task_manager::actor_action_ptr_t dispatcher_implement::create_actor(const msg_ptr_t msg_container) {
    task_manager::actor_action_ptr_t ret;
    if (NULL == msg_container) {
        return ret;
    }

    msg_type_t msg_type_id = pick_msg_type_id(msg_container);
    if (0 == msg_type_id) {
        return ret;
    }

    if (actor_action_name_map_.empty()) {
        return ret;
    }

    msg_actor_action_set_t::iterator iter = actor_action_name_map_.find(msg_type_id);
    if (actor_action_name_map_.end() == iter) {
        return ret;
    }

    return iter->second();
}

void dispatcher_implement::push_filter_to_front(msg_filter_handle_t fn) { msg_filter_list_.push_front(fn); }

void dispatcher_implement::push_filter_to_back(msg_filter_handle_t fn) { msg_filter_list_.push_back(fn); }

const std::string &dispatcher_implement::get_empty_string() {
    static std::string ret;
    return ret;
}

int dispatcher_implement::_register_action(const std::string &msg_name, task_manager::task_action_creator_t action) {
    msg_type_t msg_type_id = msg_name_to_type_id(msg_name);
    if (0 == msg_type_id) {
        WLOGERROR("%s try to register action with invalid name %s.", name(), msg_name.c_str());
        return hello::err::EN_SYS_INIT;
    }

    msg_task_action_set_t::iterator iter = task_action_name_map_.find(msg_type_id);
    if (task_action_name_map_.end() != iter) {
        WLOGERROR("%s try to register more than one task actions to type %u.", name(), msg_type_id);
        return hello::err::EN_SYS_INIT;
    }

    task_action_name_map_[msg_type_id] = action;
    return hello::err::EN_SUCCESS;
}

int dispatcher_implement::_register_action(const std::string &msg_name, task_manager::actor_action_creator_t action) {
    msg_type_t msg_type_id = msg_name_to_type_id(msg_name);
    if (0 == msg_type_id) {
        WLOGERROR("%s try to register action with invalid name %s.", name(), msg_name.c_str());
        return hello::err::EN_SYS_INIT;
    }

    msg_actor_action_set_t::iterator iter = actor_action_name_map_.find(msg_type_id);
    if (actor_action_name_map_.end() != iter) {
        WLOGERROR("%s try to register more than one actor actions to type %u.", name(), msg_type_id);
        return hello::err::EN_SYS_INIT;
    }

    actor_action_name_map_[msg_type_id] = action;
    return hello::err::EN_SUCCESS;
}