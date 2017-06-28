//
// Created by owt50 on 2016/11/14.
//

#include <protocol/pbdesc/com.const.pb.h>
#include <protocol/pbdesc/svr.const.err.pb.h>
#include <protocol/pbdesc/svr.container.pb.h>

#include <config/extern_log_categorize.h>

#include <log/log_wrapper.h>
#include <time/time_utility.h>

#include <data/player.h>

#include "actor_action_base.h"


namespace detail {
    struct actor_action_stat_guard {
        actor_action_stat_guard(actor_action_base* act): action(act) {
            if (NULL != action) {
                util::time::time_utility::update();
                start = util::time::time_utility::now();
            }
        }

        ~actor_action_stat_guard() {
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
        actor_action_base* action;
    };
}

actor_action_base::actor_action_base() : ret_code_(0), rsp_code_(0), status_(EN_AAS_CREATED), rsp_msg_disabled_(false), evt_disabled_(false) {}
actor_action_base::~actor_action_base() {
    if (EN_AAS_FINISHED != status_) {
        WLOGERROR("actor %s [%p] is created but not run", name(), this);
        set_rsp_code(hello::EN_ERR_TIMEOUT);
        set_ret_code(hello::err::EN_SYS_TIMEOUT);
    }
}

const char *actor_action_base::name() const {
    const char *ret = typeid(*this).name();
    if (NULL == ret) {
        return "RTTI Unavailable: actor_action_base";
    }

    // some compiler will generate number to mark the type
    while (ret && *ret >= '0' && *ret <= '9') {
        ++ret;
    }
    return ret;
}

int actor_action_base::hook_run() { return (*this)(request_msg_); }

int32_t actor_action_base::run(msg_type *in) {
    detail:: actor_action_stat_guard stat(this);

    if (get_status() > EN_AAS_CREATED) {
        WLOGERROR("actor %s [%p] already running", name(), this);
        return hello::err::EN_SYS_BUSY;
    }

    if (NULL != in) {
        request_msg_.Swap(in);
    }

    status_ = EN_AAS_RUNNING;
    WLOGDEBUG("actor %s [%p] start to run", name(), this);
    ret_code_ = hook_run();

    // 响应OnSuccess(这时候任务的status还是running)
    int32_t ret = 0;
    if (!evt_disabled_) {
        if (rsp_code_ < 0) {
            ret = on_failed();
            WLOGINFO("actor %s [%p] finished success but response errorcode, rsp code: %d\n", name(), this, rsp_code_);
        } else {
            ret = on_success();
        }
    }

    if (!rsp_msg_disabled_) {
        send_rsp_msg();
    }
    status_ = EN_AAS_FINISHED;
    return ret;
}

int actor_action_base::on_success() { return 0; }

int actor_action_base::on_failed() { return 0; }

hello::message_container &actor_action_base::get_request() { return request_msg_; }

const hello::message_container &actor_action_base::get_request() const { return request_msg_; }
