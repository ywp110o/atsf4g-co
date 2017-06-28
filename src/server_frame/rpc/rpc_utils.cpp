//
// Created by owent on 2016/10/4.
//

#include <log/log_wrapper.h>

#include <protocol/pbdesc/svr.container.pb.h>

#include <dispatcher/task_manager.h>

#include <protocol/pbdesc/svr.const.err.pb.h>

#include "rpc_utils.h"


namespace rpc {
    int wait(hello::message_container& msgc) {
        task_manager::task_t* task = task_manager::task_t::this_task();
        if (!task) {
            WLOGERROR("current not in a task");
            return hello::err::EN_SYS_RPC_NO_TASK;
        }

        // 协程 swap out
        void* result = NULL;
        task->yield(&result);

        // 协程 swap in

        if (cotask::EN_TS_TIMEOUT == task->get_status()) {
            return hello::err::EN_SYS_TIMEOUT;
        }

        if (task->get_status() > cotask::EN_TS_DONE) {
            return hello::err::EN_SYS_RPC_CALL;
        }

        if (NULL != result) {
            msgc.Swap(reinterpret_cast<hello::message_container*>(result));
        }

        if (msgc.has_src_server()) {
            return msgc.src_server().rpc_result();
        }

        return hello::err::EN_SUCCESS;
    }
}
