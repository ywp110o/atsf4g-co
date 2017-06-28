//
// Created by owt50 on 2016/10/9.
//

#include <log/log_wrapper.h>
#include <random/uuid_generator.h>

#include <protocol/pbdesc/svr.container.pb.h>
#include <protocol/pbdesc/svr.const.err.pb.h>

#include <dispatcher/task_manager.h>

#include "uuid.h"

namespace rpc {
    namespace db {
        namespace uuid {
            int generate(uint32_t type, std::string &uuid) {
                task_manager::task_t* task = task_manager::task_t::this_task();
                if (!task) {
                    WLOGERROR("current not in a task");
                    return hello::err::EN_SYS_RPC_NO_TASK;
                }

                // TODO 临时用标准UUID，以后改成按类型分配唯一的短数字ID
                uuid = util::random::uuid_generator::generate_string();
                return hello::err::EN_SUCCESS;
            }
        }
    }
}
