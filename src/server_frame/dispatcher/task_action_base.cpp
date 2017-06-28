//
// Created by owt50 on 2016/9/26.
//

#include <protocol/pbdesc/com.const.pb.h>
#include <protocol/pbdesc/svr.const.err.pb.h>
#include <protocol/pbdesc/svr.container.pb.h>

#include <config/extern_log_categorize.h>

#include <log/log_wrapper.h>
#include <time/time_utility.h>

#include <data/player.h>

#include "task_action_base.h"
#include "task_manager.h"

namespace detail {
    struct task_action_stat_guard {
        task_action_stat_guard(task_action_base* act): action(act) {
            if (NULL != action) {
                util::time::time_utility::update();
                start = util::time::time_utility::now();
            }
        }

        ~task_action_stat_guard() {
            if (NULL == action) {
                return;
            }

            util::time::time_utility::update();
            util::time::time_utility::raw_time_t end = util::time::time_utility::now();
            std::shared_ptr<player> user = action->get_player();
            if (user) {
                WCLOGINFO(log_categorize_t::PROTO_STAT, "%s|%s|%lldus|%d|%d",
                          action->name(), user->get_open_id().c_str(),
                          static_cast<long long>(std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()),
                          action->get_ret_code(), action->get_rsp_code()
                );
            } else {
                WCLOGINFO(log_categorize_t::PROTO_STAT, "%s|NO PLAYER|%lldus|%d|%d",
                          action->name(),
                          static_cast<long long>(std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()),
                          action->get_ret_code(), action->get_rsp_code()
                );
            }
        }

        util::time::time_utility::raw_time_t start;
        task_action_base* action;
    };
}

task_action_base::task_action_base() : task_id_(0), ret_code_(0), rsp_code_(0), rsp_msg_disabled_(false), evt_disabled_(false) {}
task_action_base::~task_action_base() {}

const char *task_action_base::name() const {
    const char *ret = typeid(*this).name();
    if (NULL == ret) {
        return "RTTI Unavailable: task_action_base";
    }

    // some compiler will generate number to mark the type
    while (ret && *ret >= '0' && *ret <= '9') {
        ++ret;
    }
    return ret;
}

int task_action_base::operator()(void *priv_data) {
    detail::task_action_stat_guard stat(this);

    if (NULL != priv_data) {
        request_msg_.Swap(reinterpret_cast<hello::message_container *>(priv_data));
    }

    task_manager::task_t *task = cotask::this_task::get<task_manager::task_t>();
    if (NULL == task) {
        WLOGERROR("task convert failed, must in task.");
        return hello::err::EN_SYS_INIT;
    }

    task_id_ = task->get_id();
    ret_code_ = hook_run();

    if (evt_disabled_) {
        if (ret_code_ < 0) {
            WLOGERROR("task %s [0x%llx] without evt ret code %d, rsp code %d\n", name(), get_task_id_llu(), ret_code_, rsp_code_);
        } else {
            WLOGDEBUG("task %s [0x%llx] without evt ret code %d, rsp code %d\n", name(), get_task_id_llu(), ret_code_, rsp_code_);
        }

        if (!rsp_msg_disabled_) {
            send_rsp_msg();
        }
        return ret_code_;
    }
    // 响应OnSuccess(这时候任务的status还是running)
    if (cotask::EN_TS_RUNNING == task->get_status() && ret_code_ >= 0) {
        int ret = 0;
        if (rsp_code_ < 0) {
            ret = on_failed();
            WLOGINFO("task %s [0x%llx] finished success but response errorcode, rsp code: %d\n", name(), get_task_id_llu(), rsp_code_);
        } else {
            ret = on_success();
        }
        if (!rsp_msg_disabled_) {
            send_rsp_msg();
        }
        return ret;
    }

    if (hello::err::EN_SUCCESS == ret_code_) {
        ret_code_ = hello::err::EN_SYS_UNKNOWN;
    }

    if (hello::EN_SUCCESS == rsp_code_) {
        rsp_code_ = hello::EN_ERR_UNKNOWN;
    }

    WLOGERROR("task %s [0x%llx] ret code %d, rsp code %d\n", name(), get_task_id_llu(), ret_code_, rsp_code_);

    // 响应OnTimeout
    if (cotask::EN_TS_TIMEOUT == task->get_status()) {
        on_timeout();
    }

    // 如果不是running且不是timeout，可能是其他原因被kill掉了，响应OnFailed
    int ret = on_failed();
    if (!rsp_msg_disabled_) {
        send_rsp_msg();
    }
    return ret;
}

int task_action_base::hook_run() { return (*this)(request_msg_); }

int task_action_base::on_success() { return 0; }

int task_action_base::on_failed() { return 0; }

int task_action_base::on_timeout() { return 0; }

uint64_t task_action_base::get_task_id() const { return task_id_; }

unsigned long long task_action_base::get_task_id_llu() const { return static_cast<unsigned long long>(task_id_); }

hello::message_container &task_action_base::get_request() { return request_msg_; }

const hello::message_container &task_action_base::get_request() const { return request_msg_; }
