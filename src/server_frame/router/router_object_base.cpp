//
// Created by owent on 2018/05/01.
//

#include <config/logic_config.h>
#include <log/log_wrapper.h>
#include <time/time_utility.h>


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
    : key_(k), loading_cache_task_(0), loading_object_task_(0), last_save_time_(0), last_visit_time_(0), router_svr_id_(0), router_svr_ver_(0),
      timer_sequence_(0), flags_(0) {

    // 创建时初始化最后访问时间
    refresh_visit_time();
}

router_object_base::router_object_base(key_t &&k)
    : key_(k), loading_cache_task_(0), loading_object_task_(0), last_save_time_(0), last_visit_time_(0), router_svr_id_(0), router_svr_ver_(0),
      timer_sequence_(0), flags_(0) {

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
    if (0 != loading_cache_task_) {
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
    if (0 != loading_object_task_) {
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

int router_object_base::pull_cache_inner(void *priv_data) {
    // TODO 排队读任务
    return pull_cache(priv_data);
}

int router_object_base::pull_object_inner(void *priv_data) {
    // TODO 排队读任务
    return pull_object(priv_data);
}

int router_object_base::save_object_inner(void *priv_data) {
    // TODO 排队写任务
    return save_object(priv_data);
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