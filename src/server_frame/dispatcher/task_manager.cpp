//
// Created by owt50 on 2016/9/26.
//

#include <log/log_wrapper.h>
#include <protocol/pbdesc/svr.const.err.pb.h>

#include <config/logic_config.h>

#include "task_manager.h"

task_manager::task_manager() {}

task_manager::~task_manager() {}

int task_manager::init() {
    native_mgr_ = mgr_t::create();
    stack_pool_ = stack_pool_t::create();

    if (logic_config::me()->get_cfg_logic().task_stack_size > 0) {
        stack_pool_->set_stack_size(logic_config::me()->get_cfg_logic().task_stack_size);
    }

    return 0;
}

int task_manager::reload() {
    if (stack_pool_ && logic_config::me()->get_cfg_logic().task_stack_size > 0) {
        stack_pool_->set_stack_size(logic_config::me()->get_cfg_logic().task_stack_size);
    }

    return 0;
}

int task_manager::start_task(id_t task_id, start_data_t &data) {
    int res = native_mgr_->start(task_id, &data);
    if (res < 0) {
        WLOGERROR("start task 0x%llx failed.", static_cast<unsigned long long>(task_id));

        // 错误码
        return hello::err::EN_SYS_NOTFOUND;
    }

    return 0;
}

int task_manager::resume_task(id_t task_id, resume_data_t &data) {
    int res = native_mgr_->resume(task_id, &data);
    if (res < 0) {
        WLOGERROR("resume task 0x%llx failed.", static_cast<unsigned long long>(task_id));

        // 错误码
        return hello::err::EN_SYS_NOTFOUND;
    }

    return 0;
}

int task_manager::tick(time_t sec, int nsec) {
    native_mgr_->tick(sec, nsec);
    return 0;
}

task_manager::task_ptr_t task_manager::get_task(id_t task_id) {
    if (!native_mgr_) {
        return task_manager::task_ptr_t();
    }

    return native_mgr_->find_task(task_id);
}

size_t task_manager::get_stack_size() const { return logic_config::me()->get_cfg_logic().task_stack_size; }

int task_manager::add_task(const task_t::ptr_t &task, time_t timeout) {
    int res = 0;
    if (0 == timeout) {
        // read default timeout from configure
        res = native_mgr_->add_task(task, logic_config::me()->get_cfg_logic().task_csmsg_timeout, 0);
    } else {
        res = native_mgr_->add_task(task, timeout, 0);
    }

    if (res < 0) {
        WLOGERROR("add task failed, res: %d", res);
        return hello::err::EN_SYS_PARAM;
    }

    return hello::err::EN_SUCCESS;
}

int task_manager::report_create_error(const char *fn_name) {
    WLOGERROR("[%s] create task failed. current task number=%u", fn_name, static_cast<uint32_t>(native_mgr_->get_task_size()));
    return hello::err::EN_SYS_MALLOC;
}