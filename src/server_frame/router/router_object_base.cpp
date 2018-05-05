//
// Created by owent on 2018/05/01.
//

#include <config/logic_config.h>
#include <log/log_wrapper.h>
#include <time/time_utility.h>


#include <protocol/pbdesc/svr.const.err.pb.h>

#include "router_object_base.h"


router_object_base::flag_guard::flag_guard(router_object_base &owner, int f) : owner_(&owner), f_(f) {
    if (f_ & owner_->get_flags()) {
        f_ = 0;
    } else if (0 != f_) {
        owner_->set_flag(f_);
    }
}

router_object_base::flag_guard::~flag_guard() {
    if (NULL != owner_ && 0 != f_) {
        owner_->unset_flag(f_);
    }
}

router_object_base::router_object_base(const key_t &k)
    : key_(k), last_save_time_(0), last_visit_time_(0), router_svr_id_(0), router_svr_ver_(0), timer_sequence_(0), saving_sequence_(0), saved_sequence_(0),
      flags_(0) {

    // 创建时初始化最后访问时间
    refresh_visit_time();
}

router_object_base::router_object_base(key_t &&k)
    : key_(k), last_save_time_(0), last_visit_time_(0), router_svr_id_(0), router_svr_ver_(0), timer_sequence_(0), flags_(0) {

    // 创建时初始化最后访问时间
    refresh_visit_time();
}

router_object_base::~router_object_base() {}

void router_object_base::refresh_visit_time() { last_visit_time_ = util::time::time_utility::get_now(); }

void router_object_base::refresh_save_time() { last_save_time_ = util::time::time_utility::get_now(); }

int router_object_base::remove_object(void *priv_data) {
    if (!is_writable()) {
        return hello::EN_SUCCESS;
    }

    // 移除实体需要设置路由BUS ID为0并保存一次
    set_router_server_id(0, get_router_version() + 1);
    refresh_visit_time();

    int ret = save(priv_data);
    if (ret < 0) {
        WLOGERROR("remove router object %u:0x%llx", get_key().type_id, get_key().object_id_ull());
        return ret;
    }

    unset_flag(flag_t::EN_ROFT_WRITABLE);
    if (is_pulling_object()) {
        set_flag(flag_t::EN_ROFT_OBJECT_REMOVED);
    }

    return ret;
}

bool router_object_base::is_cache_available() const {
    if (is_pulling_cache()) {
        return false;
    }

    if (is_writable()) {
        return true;
    }

    if (last_save_time_ + logic_config::me()->get_cfg_logic().router.cache_update_interval < util::time::time_utility::get_now()) {
        return false;
    }

    return true;
}

bool router_object_base::is_object_available() const {
    if (is_pulling_object()) {
        return false;
    }

    return is_writable();
}

int router_object_base::pull_cache(void *priv_data) { return pull_object(priv_data); }

int router_object_base::upgrade() {
    if (check_flag(flag_t::EN_ROFT_WRITABLE)) {
        return 0;
    }

    refresh_visit_time();
    set_flag(flag_t::EN_ROFT_WRITABLE);
    unset_flag(flag_t::EN_ROFT_OBJECT_REMOVED);
    return 0;
}

int router_object_base::downgrade() {
    if (!check_flag(flag_t::EN_ROFT_WRITABLE)) {
        return 0;
    }

    refresh_visit_time();
    unset_flag(flag_t::EN_ROFT_WRITABLE);
    return 0;
}

int router_object_base::await_pull_task(task_manager::task_ptr_t &self_task) {
    if (!self_task) {
        return hello::err::EN_SYS_RPC_NO_TASK;
    }

    size_t left_ttl = logic_config::me()->get_cfg_logic().router.retry_max_ttl;
    // 默认retry 5 次
    if (0 == left_ttl) {
        left_ttl = 5;
    }

    for (; left_ttl > 0; --left_ttl) {
        if (!pulling_task_ || pulling_task_ == self_task) {
            return 0;
        }

        if (pulling_task_->is_exiting()) {
            pulling_task_.reset();
            continue;
        }

        pulling_task_->next(self_task);
        self_task->yield(NULL);

        if (self_task->is_timeout()) {
            return hello::err::EN_SYS_TIMEOUT;
        } else if (self_task->is_exiting()) {
            return hello::err::EN_SYS_RPC_TASK_EXITING;
        }
    }

    // 超出重试次数限制
    return hello::err::EN_ROUTER_TTL_EXTEND;
}

int router_object_base::pull_cache_inner(void *priv_data) {
    task_manager::task_ptr_t self_task(task_manager::task_t::this_task());
    if (!self_task) {
        return hello::err::EN_SYS_RPC_NO_TASK;
    }

    int ret = await_pull_task(self_task);
    if (ret < 0) {
        return ret;
    }

    // 执行读任务
    pulling_task_.swap(self_task);
    ret = pull_cache(priv_data);
    pulling_task_.reset();

    if (ret < 0) {
        return ret;
    }

    // 拉取成功要refresh_save_time
    refresh_save_time();

    return ret;
}

int router_object_base::pull_object_inner(void *priv_data) {
    task_manager::task_ptr_t self_task(task_manager::task_t::this_task());
    if (!self_task) {
        return hello::err::EN_SYS_RPC_NO_TASK;
    }

    int ret = await_pull_task(self_task);
    if (ret < 0) {
        return ret;
    }

    // 执行读任务
    pulling_task_.swap(self_task);
    ret = pull_object(priv_data);
    pulling_task_.reset();

    if (ret < 0) {
        return ret;
    }

    // 拉取成功要refresh_save_time
    refresh_save_time();

    if (0 != get_router_server_id()) {
        if (logic_config::me()->get_self_bus_id() != get_router_server_id()) {
            // 可能某处的缓存过老，这是正常流程，返回错误码即可，不用打错误日志
            return hello::err::EN_ROUTER_NOT_WRITABLE;
        }
    }

    return ret;
}

int router_object_base::save_object_inner(void *priv_data) {

    // 排队写任务和并发写merge
    uint64_t this_saving_seq = ++saving_sequence_;

    task_manager::task_ptr_t self_task(task_manager::task_t::this_task());
    if (!self_task) {
        return hello::err::EN_SYS_RPC_NO_TASK;
    }

    // 如果有其他任务正在保存，需要等待那个任务结束
    // 因为可能叠加和倍其他任务抢占，所以这里需要重试到有结果（失败或等待）
    while (true) {
        // 当前任务不可执行
        if (self_task->is_exiting()) {
            if (self_task->is_timeout()) {
                return hello::err::EN_SYS_TIMEOUT;
            } else {
                return hello::err::EN_SYS_RPC_TASK_EXITING;
            }
        }

        if (!is_writable()) {
            return hello::err::EN_ROUTER_NOT_WRITABLE;
        }

        // 如果其他任务的保存涵盖了自己的数据变化，则直接成功返回
        if (saved_sequence_ >= this_saving_seq) {
            return 0;
        }

        // 自己是保存任务
        if (!saving_task_) {
            break;
        }

        if (saving_task_ != self_task) {
            // fallback，理论上不会出现这个，下面会reset的
            if (saving_task_->is_exiting()) {
                saving_task_.reset();
            } else {
                saving_task_->next(self_task);
                self_task->yield(NULL);

                if (self_task->is_timeout()) {
                    return hello::err::EN_SYS_TIMEOUT;
                } else if (self_task->is_exiting()) {
                    return hello::err::EN_SYS_RPC_TASK_EXITING;
                }
            }
        } else {
            break;
        }
    }

    uint64_t real_saving_seq = saving_sequence_;
    saving_task_.swap(self_task);
    int ret = save_object(priv_data);
    saving_task_.reset();

    if (ret >= 0 && real_saving_seq > saved_sequence_) {
        saved_sequence_ = real_saving_seq;
    } else if (ret < 0) {
        // 保存失败
        return ret;
    }

    refresh_save_time();
    return ret;
}


namespace std {
    bool operator==(const router_object_base::key_t &l, const router_object_base::key_t &r) UTIL_CONFIG_NOEXCEPT {
        return l.object_id == r.object_id && l.type_id == r.type_id;
    }

    bool operator<(const router_object_base::key_t &l, const router_object_base::key_t &r) UTIL_CONFIG_NOEXCEPT {
        if (l.type_id != r.type_id) {
            return l.type_id < r.type_id;
        }

        return l.object_id < r.object_id;
    }
} // namespace std