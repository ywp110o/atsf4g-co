//
// Created by owent on 2018/05/01.
//

#include <config/logic_config.h>
#include <log/log_wrapper.h>
#include <time/time_utility.h>

#include <protocol/pbdesc/svr.const.err.pb.h>

#include <dispatcher/ss_msg_dispatcher.h>
#include <dispatcher/task_manager.h>


#include "action/task_action_auto_save_objects.h"
#include "action/task_action_router_transfer.h"
#include "action/task_action_router_update.h"

#include "router_manager_base.h"
#include "router_manager_set.h"
#include "router_object_base.h"

// #include "router_guild_manager.h"
// #include "router_player_group_manager.h"
// #include "router_player_manager.h"
// #include "router_team_manager.h"


router_manager_set::router_manager_set() : last_proc_time_(0) { memset(mgrs_, 0, sizeof(mgrs_)); }

int router_manager_set::init() {
    int ret = 0;

    // 注册路由系统的内部事件
    REG_TASK_MSG_HANDLE(ss_msg_dispatcher, ret, task_action_router_transfer, hello::SSMsgBody::kMssRouterTransferReq);
    REG_TASK_MSG_HANDLE(ss_msg_dispatcher, ret, task_action_router_update, hello::SSMsgBody::kMssRouterUpdateSync);

    return ret;
}

int router_manager_set::tick() {
    int ret = 0;

    // 每秒只需要判定一次
    if (last_proc_time_ == ::util::time::time_utility::get_now()) {
        return ret;
    }
    // 每分钟打印一次统计数据
    if (last_proc_time_ / util::time::time_utility::MINITE_SECONDS != ::util::time::time_utility::get_now() / util::time::time_utility::MINITE_SECONDS) {
        if (timer_list_.empty()) {
            WLOGINFO("[STAT] router manager set: timer count 0, save pending list count %llu", static_cast<unsigned long long>(save_list_.size()));
        } else {
            WLOGINFO("[STAT] router manager set: timer count %llu(next active at %lld), save pending list count %llu",
                     static_cast<unsigned long long>(timer_list_.size()), static_cast<long long>(timer_list_.front().timeout),
                     static_cast<unsigned long long>(save_list_.size()));
        }
        for (int i = 0; i < hello::EnRouterObjectType_ARRAYSIZE; ++i) {
            if (mgrs_[i]) {
                WLOGINFO("[STAT] \t%s has %llu caches", mgrs_[i]->name(), static_cast<unsigned long long>(mgrs_[i]->size()));
            }
        }
    }
    last_proc_time_ = ::util::time::time_utility::get_now();

    time_t cache_expire = logic_config::me()->get_cfg_logic().router.cache_free_timeout;
    time_t object_expire = logic_config::me()->get_cfg_logic().router.object_free_timeout;
    time_t object_save = logic_config::me()->get_cfg_logic().router.object_save_interval;
    // 缓存失效定时器
    do {
        if (timer_list_.empty()) {
            break;
        }

        timer_t &cache = timer_list_.front();

        // 如果没到时间，后面的全没到时间
        if (last_proc_time_ <= cache.timeout) {
            break;
        }

        // 如果已下线并且缓存失效则跳过
        std::shared_ptr<router_object_base> obj = cache.obj_watcher.lock();
        if (!obj) {
            timer_list_.pop_front();
            continue;
        }

        // 如果操作序列失效则跳过
        if (false == obj->check_timer_sequence(cache.timer_sequence)) {
            timer_list_.pop_front();
            continue;
        }

        // 已销毁则跳过
        router_manager_base *mgr = get_manager(cache.type_id);
        if (NULL == mgr) {
            timer_list_.pop_front();
            continue;
        }

        bool cache_expired = false;
        if (obj->is_writable()) {
            // 正在拉取数据则不需要启动保存流程
            if (false == obj->is_pulling_object()) {
                // 实体过期
                if (obj->get_last_visit_time() + object_expire < last_proc_time_) {
                    save_list_.push_back(auto_save_data_t());
                    auto_save_data_t &auto_save = save_list_.back();
                    auto_save.object = obj;
                    auto_save.type_id = cache.type_id;
                    auto_save.action = EN_ASA_REMOVE_OBJECT;
                } else if (obj->get_last_save_time() + object_save < last_proc_time_) { // 实体保存
                    save_list_.push_back(auto_save_data_t());
                    auto_save_data_t &auto_save = save_list_.back();
                    auto_save.object = obj;
                    auto_save.type_id = cache.type_id;
                    auto_save.action = EN_ASA_SAVE;
                    obj->refresh_save_time();
                }
            }
        } else {
            // 缓存过期
            if (false == obj->is_pulling_cache() && false == obj->is_pulling_object() && obj->get_last_visit_time() + cache_expire < last_proc_time_) {
                cache_expired = true;
                save_list_.push_back(auto_save_data_t());
                auto_save_data_t &auto_save = save_list_.back();
                auto_save.object = obj;
                auto_save.type_id = cache.type_id;
                auto_save.action = EN_ASA_REMOVE_CACHE;
            }
        }

        if (!cache_expired) {
            insert_timer(mgr, obj);
        }
        timer_list_.pop_front();
        ++ret;
    } while (true);

    if (!save_list_.empty() && false == is_save_task_running()) {
        task_manager::id_t tid = 0;
        task_manager::me()->create_task_with_timeout<task_action_auto_save_objects>(tid, logic_config::me()->get_cfg_logic().task_nomsg_timeout,
                                                                                    task_action_auto_save_objects::ctor_param_t());
        if (0 == tid) {
            WLOGERROR("create task_action_auto_save_objects failed");
        } else {
            dispatcher_start_data_t start_data;
            start_data.private_data = NULL;
            start_data.message.msg_addr = NULL;
            start_data.message.msg_type = 0;

            if (0 == task_manager::me()->start_task(tid, start_data)) {
                save_task_ = task_manager::me()->get_task(tid);
            }
        }
    }

    return ret;
}

bool router_manager_set::insert_timer(router_manager_base *mgr, const std::shared_ptr<router_object_base> &obj) {
    if (last_proc_time_ <= 0) {
        WLOGERROR("router_manager_set not actived");
    }
    assert(last_proc_time_ > 0);
    if (!obj || !mgr) {
        return false;
    }

    router_manager_base *checked_mgr = get_manager(mgr->get_type_id());
    if (checked_mgr != mgr) {
        WLOGERROR("router_manager_set has registered %u to %s, but try to setup timer of %s", mgr->get_type_id(),
                  (NULL == checked_mgr ? "None" : checked_mgr->name()), mgr->name());
        return false;
    }

    timer_list_.push_back(timer_t());
    timer_t &tm = timer_list_.back();
    tm.obj_watcher = obj;
    tm.type_id = mgr->get_type_id();
    tm.timeout = util::time::time_utility::get_now() + logic_config::me()->get_cfg_logic().router.timer_interval;
    tm.timer_sequence = obj->alloc_timer_sequence();
    return true;
}

router_manager_base *router_manager_set::get_manager(uint32_t type) {
    if (type >= hello::EnRouterObjectType_ARRAYSIZE) {
        return nullptr;
    }

    return mgrs_[type];
}

int router_manager_set::register_manager(router_manager_base *b) {
    if (NULL == b) {
        return hello::err::EN_SYS_PARAM;
    }

    uint32_t type = b->get_type_id();
    if (type >= hello::EnRouterObjectType_ARRAYSIZE) {
        WLOGERROR("router %s has invalid type id %u", b->name(), type);
        return hello::err::EN_ROUTER_TYPE_INVALID;
    }

    if (mgrs_[type]) {
        WLOGERROR("router %s has type conflicy with %s", mgrs_[type]->name(), b->name());
        return hello::err::EN_ROUTER_TYPE_CONFLICT;
    }

    mgrs_[type] = b;
    return hello::err::EN_SUCCESS;
}

int router_manager_set::unregister_manager(router_manager_base *b) {
    if (NULL == b) {
        return hello::err::EN_SYS_PARAM;
    }

    uint32_t type = b->get_type_id();
    if (type >= hello::EnRouterObjectType_ARRAYSIZE) {
        WLOGERROR("router %s has invalid type id %u", b->name(), type);
        return hello::err::EN_ROUTER_TYPE_INVALID;
    }

    if (mgrs_[type] == b) {
        mgrs_[type] = NULL;
    }

    return hello::err::EN_SUCCESS;
}

bool router_manager_set::is_save_task_running() const { return save_task_ && !save_task_->is_exiting(); }
