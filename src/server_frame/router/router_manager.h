//
// Created by owent on 2018/05/01.
//

#ifndef ROUTER_ROUTER_MANAGER_H
#define ROUTER_ROUTER_MANAGER_H

#pragma once

#include <functional>
#include <unordered_map>
#include <vector>

#include <libcotask/task.h>

#include <config/logic_config.h>
#include <log/log_wrapper.h>

#include <rpc/router/router_object_base.h>

#include "router_manager_base.h"
#include "router_manager_set.h"
#include <config/logic_config.h>
#include <dispatcher/task_manager.h>
#include <protocol/pbdesc/svr.const.err.pb.h>


template <typename TCache, typename TObj, typename TPrivData>
class router_manager : public router_manager_base {
public:
    typedef TCache cache_t;
    typedef TPrivData priv_data_t;
    typedef router_manager<TCache, TObj, TPrivData> self_type;
    typedef typename cache_t::key_t key_t;
    typedef typename cache_t::flag_t flag_t;
    typedef typename cache_t::object_ptr_t object_ptr_t;
    typedef typename cache_t::ptr_t ptr_t;

    typedef std::function<void(self_type &, const key_t &, const ptr_t &, priv_data_t)> remove_fn_t;
    typedef std::function<void(self_type &, const ptr_t &, priv_data_t)> pull_fn_t;

    typedef std::weak_ptr<cache_t> store_ptr_t;

public:
    explicit router_manager(uint32_t type_id) : router_manager_base(type_id) {}

    /**
     * @brief 拉取缓存对象，如果不存在返回空
     * @param key
     * @return 缓存对象
     */
    virtual std::shared_ptr<router_object_base> get_base_cache(const key_t &key) const UTIL_CONFIG_OVERRIDE {
        typename std::unordered_map<key_t, ptr_t>::const_iterator iter = caches_.find(key);
        if (iter == caches_.end()) {
            return nullptr;
        }

        return std::dynamic_pointer_cast<router_object_base>(get_cache(key));
    }

    ptr_t get_cache(const key_t &key) const {
        typename std::unordered_map<key_t, ptr_t>::const_iterator iter = caches_.find(key);
        if (iter == caches_.end()) {
            return nullptr;
        }

        return iter->second;
    }

    ptr_t get_object(const key_t &key) const {
        typename std::unordered_map<key_t, ptr_t>::const_iterator iter = caches_.find(key);
        if (iter == caches_.end() || !iter->second->is_writable()) {
            return nullptr;
        }

        return iter->second;
    }

    virtual int mutable_cache(std::shared_ptr<router_object_base> &out, const key_t &key, void *priv_data) UTIL_CONFIG_OVERRIDE {
        ptr_t outc;
        int ret = mutable_cache(outc, key, reinterpret_cast<priv_data_t>(priv_data));
        out = std::dynamic_pointer_cast<router_object_base>(outc);
        return ret;
    }

    virtual int mutable_cache(ptr_t &out, const key_t &key, priv_data_t priv_data) {
        size_t left_ttl = logic_config::me()->get_cfg_logic().router.retry_max_ttl;
        for (; left_ttl > 0; --left_ttl) {
            out = get_cache(key);
            if (out && out->is_cache_available()) {
                return hello::err::EN_SUCCESS;
            }

            if (!out) {
                out = std::make_shared<cache_t>(key);
                if (!out) {
                    return hello::err::EN_SYS_MALLOC;
                }

                if (!insert(key, out)) {
                    return hello::err::EN_SYS_PARAM;
                }
            }

            task_manager::task_t *task = task_manager::task_t::this_task();
            if (!task) {
                WLOGERROR("current not in a task");
                return hello::err::EN_SYS_RPC_NO_TASK;
            }

            // wait for object task
            task_manager::task_ptr_t wait_for;
            if (0 != out->get_object_task()) {
                wait_for = task_manager::me()->get_task(out->get_object_task());
                if (wait_for->is_completed()) {
                    wait_for.reset();
                }
            }

            // wait for cache task
            if (!wait_for && 0 != out->get_cache_task()) {
                wait_for = task_manager::me()->get_task(out->get_cache_task());
                if (wait_for->is_completed()) {
                    wait_for.reset();
                }
            }

            if (wait_for) {
                wait_for->next(task->shared_from_this());
                task->yield(nullptr);
                continue;
            }

            // pull using TYPE's API
            setup_cache_task(*out, task->get_id());
            int res = out->pull_cache_inner(reinterpret_cast<void *>(priv_data));
            setup_cache_task(*out, 0);

            if (res < 0) {
                if (cotask::EN_TS_TIMEOUT == task->get_status()) {
                    return hello::err::EN_SYS_TIMEOUT;
                }
                continue;
            }

            // 拉取成功要refresh_save_time
            out->refresh_save_time();

            on_evt_pull_cache(out, priv_data);
            return hello::err::EN_SUCCESS;
        }

        // 超出重试次数限制
        return hello::err::EN_ROUTER_TTL_EXTEND;
    }

    /**
     * @brief 检查缓存是否有效，如果过期则尝试拉取一次
     * @param in 输入保存的对象,如果有更新并且更新成功，会自动重设这个变量
     * @param key 重新拉取缓存时的key
     * @param out 输出对象指针
     * @param priv_data
     * @return
     */
    int renew_cache(store_ptr_t &in, ptr_t &out, const key_t &key, priv_data_t priv_data) {
        if (!in.expired()) {
            out = in.lock();
        } else {
            out = nullptr;
        }

        if (out && !out->check_flag(flag_t::EN_ROFT_CACHE_REMOVED)) {
            return hello::err::EN_SUCCESS;
        }

        int ret = mutable_cache(out, key, priv_data);
        if (ret >= 0) {
            in = out;
        }

        return ret;
    }

    virtual int mutable_object(std::shared_ptr<router_object_base> &out, const key_t &key, void *priv_data) UTIL_CONFIG_OVERRIDE {
        ptr_t outc;
        int ret = mutable_object(outc, key, reinterpret_cast<priv_data_t>(priv_data));
        out = std::dynamic_pointer_cast<router_object_base>(outc);
        return ret;
    }

    virtual int mutable_object(ptr_t &out, const key_t &key, priv_data_t priv_data) {
        size_t left_ttl = logic_config::me()->get_cfg_logic().router.retry_max_ttl;
        for (; left_ttl > 0; --left_ttl) {
            out = get_cache(key);
            if (out && out->is_object_available()) {
                return hello::err::EN_SUCCESS;
            }

            if (!out) {
                out = std::make_shared<cache_t>(key);
                if (!out) {
                    return hello::err::EN_SYS_MALLOC;
                }

                if (!insert(key, out)) {
                    return hello::err::EN_SYS_PARAM;
                }
            }

            task_manager::task_t *task = task_manager::task_t::this_task();
            if (!task) {
                WLOGERROR("current not in a task");
                return hello::err::EN_SYS_RPC_NO_TASK;
            }

            // wait for object task
            task_manager::task_ptr_t wait_for;
            if (0 != out->get_object_task()) {
                wait_for = task_manager::me()->get_task(out->get_object_task());
                if (wait_for->is_completed()) {
                    wait_for.reset();
                }
            }

            if (wait_for) {
                wait_for->next(task->shared_from_this());
                task->yield(nullptr);
                continue;
            }

            // pull using TYPE's API
            setup_object_task(*out, task->get_id());
            out->unset_flag(router_object_base::flag_t::EN_ROFT_OBJECT_REMOVED);
            out->unset_flag(router_object_base::flag_t::EN_ROFT_FORCE_PULL_OBJECT);
            int res = out->pull_object_inner(reinterpret_cast<void *>(priv_data));
            setup_object_task(*out, 0);

            if (res < 0) {
                if (cotask::EN_TS_TIMEOUT == task->get_status()) {
                    return hello::err::EN_SYS_TIMEOUT;
                }
                continue;
            }

            out->refresh_save_time();
            if (0 != out->get_router_server_id()) {
                if (logic_config::me()->get_self_bus_id() != out->get_router_server_id()) {
                    // 可能某处的缓存过老，这是正常流程，返回错误码即可，不用打错误日志
                    return hello::err::EN_ROUTER_NOT_WRITABLE;
                }
            }

            // 拉取成功要refresh_save_time
            // 如果中途被移除，则降级回缓存
            if (!out->check_flag(router_object_base::flag_t::EN_ROFT_OBJECT_REMOVED)) {
                out->upgrade(); // 升级为缓存

                on_evt_pull_object(out, priv_data);
                return hello::err::EN_SUCCESS;
            }
        }

        // 超出重试次数限制
        return hello::err::EN_ROUTER_TTL_EXTEND;
    }

    virtual int transfer(const key_t &key, uint64_t svr_id, bool need_notify, priv_data_t priv_data) {
        ptr_t obj;
        int ret = mutable_object(obj, key, priv_data);
        if (ret < 0 || !obj) {
            return ret;
        }

        return transfer(obj, svr_id, need_notify, priv_data);
    }

    virtual int transfer(const ptr_t &obj, uint64_t svr_id, bool need_notify, priv_data_t priv_data) {
        if (!obj) {
            return hello::err::EN_SYS_PARAM;
        }

        if (!obj->is_writable()) {
            return hello::err::EN_ROUTER_NOT_WRITABLE;
        }

        if (svr_id == obj->get_router_server_id()) {
            return hello::err::EN_SUCCESS;
        }

        // 正在转移中
        router_object_base::flag_guard transfer_flag(*obj, router_object_base::flag_t::EN_ROFT_TRANSFERING);
        if (!transfer_flag) {
            return hello::err::EN_ROUTER_TRANSFER;
        }

        // 保存到数据库
        uint32_t old_v = obj->get_router_version();
        obj->set_router_server_id(svr_id, old_v + 1);
        int ret = obj->save(reinterpret_cast<void *>(priv_data));

        // 数据库失败要强制拉取
        if (ret < 0) {
            if (old_v + 1 == obj->get_router_version()) {
                obj->set_router_server_id(logic_config::me()->get_self_bus_id(), old_v);
            }
            obj->unset_flag(router_object_base::flag_t::EN_ROFT_WRITABLE);

            // TODO 如果转发不成功，要回发执行失败
            return ret;
        }

        obj->downgrade();
        on_evt_remove_object(obj->get_key(), obj, reinterpret_cast<priv_data_t>(priv_data));

        if (0 != svr_id && need_notify) {
            // 如果目标不是0则通知目标服务器
            hello::SSRouterTransferReq req;
            hello::SSRouterTransferRsp rsp;
            hello::SSRouterHead *router_head = req.mutable_object();
            if (NULL != router_head) {
                router_head->set_router_src_bus_id(obj->get_router_server_id());
                router_head->set_router_version(obj->get_router_version());

                router_head->set_object_inst_id(obj->get_key().object_id);
                router_head->set_object_type_id(get_type_id());

                ret = rpc::router::robj::send_transfer(svr_id, req, rsp);
                if (ret < 0) {
                    WLOGERROR("transfer router object (type=%u) 0x%llx failed, res: %d", get_type_id(), obj->get_key().object_id_ull(), ret);
                }
            } else {
                ret = hello::err::EN_SYS_MALLOC;
            }
        }

        // 如果转发成功，要转发这期间收到的消息
        if (ret >= 0) {
            while (!obj->get_transfer_pending_list().empty()) {
                std::list<hello::SSMsg> all_msgs;
                all_msgs.swap(obj->get_transfer_pending_list());

                for (auto &msg : all_msgs) {
                    int res = send_msg_raw(*obj, msg);
                    if (res < 0) {
                        WLOGERROR("transfer router object (type=%u) 0x%llx message failed, res: %d", get_type_id(), obj->get_key().object_id_ull(), res);
                    }
                }
            }
        }
        // TODO 如果转发不成功，要回发执行失败
        return ret;
    }

    virtual bool remove_cache(const key_t &key, std::shared_ptr<router_object_base> cache, void *priv_data) UTIL_CONFIG_OVERRIDE {
        typename std::unordered_map<key_t, ptr_t>::iterator iter;
        if (!cache) {
            iter = caches_.find(key);
        } else {
            iter = caches_.find(cache->get_key());
        }

        if (iter == caches_.end()) {
            return false;
        }

        on_evt_remove_cache(key, iter->second, reinterpret_cast<priv_data_t>(priv_data));

        iter->second->set_flag(router_object_base::flag_t::EN_ROFT_CACHE_REMOVED);
        caches_.erase(iter);
        stat_size_ = caches_.size();
        return true;
    }

    virtual bool remove_object(const key_t &key, std::shared_ptr<router_object_base> cache, void *priv_data) UTIL_CONFIG_OVERRIDE {
        ptr_t cache_child;
        if (!cache) {
            typename std::unordered_map<key_t, ptr_t>::iterator iter = caches_.find(key);
            if (iter == caches_.end()) {
                return false;
            }

            cache_child = iter->second;
            cache = std::dynamic_pointer_cast<router_object_base>(iter->second);
            assert(!!cache);
        } else {
            cache_child = std::dynamic_pointer_cast<cache_t>(cache);
            assert(!!cache_child);
        }

        if (!cache_child) {
            return false;
        }

        if (cache->remove_object(reinterpret_cast<priv_data_t>(priv_data)) < 0) {
            return false;
        }

        on_evt_remove_object(key, cache_child, reinterpret_cast<priv_data_t>(priv_data));
        return true;
    }

    void set_on_remove_object(remove_fn_t &&fn) { handle_on_remove_object_ = fn; }
    void set_on_remove_object(const remove_fn_t &fn) { handle_on_remove_object_ = fn; }

    void set_on_remove_cache(remove_fn_t &&fn) { handle_on_remove_cache_ = fn; }
    void set_on_remove_cache(const remove_fn_t &fn) { handle_on_remove_cache_ = fn; }


    void set_on_pull_object(pull_fn_t &&fn) { handle_on_pull_object_ = fn; }
    void set_on_pull_object(const pull_fn_t &fn) { handle_on_pull_object_ = fn; }

    void set_on_pull_cache(pull_fn_t &&fn) { handle_on_pull_cache_ = fn; }
    void set_on_pull_cache(const pull_fn_t &fn) { handle_on_pull_cache_ = fn; }


    void foreach (const std::function<bool(const ptr_t &)> fn) {
        if (!fn) {
            return;
        }

        // 先复制出所有的只能指针，防止回掉过程中成员变化带来问题
        std::vector<ptr_t> res;
        res.reserve(caches_.size());
        for (auto &che : caches_) {
            res.push_back(che.second);
        }

        for (auto &p : res) {
            if (!fn(p)) {
                break;
            }
        }
    }

private:
    bool insert(const key_t &key, const ptr_t &d) {
        if (!d || caches_.find(key) != caches_.end()) {
            return false;
        }

        // 插入定时器
        if (!router_manager_set::me()->insert_timer(this, d)) {
            return false;
        }

        caches_[key] = d;
        stat_size_ = caches_.size();
        return true;
    }

protected:
    // =============== event =================
    virtual void on_evt_remove_cache(const key_t &key, const ptr_t &cache, priv_data_t priv_data) {
        if (handle_on_remove_cache_) {
            handle_on_remove_cache_(*this, key, cache, priv_data);
        }
    }

    virtual void on_evt_remove_object(const key_t &key, const ptr_t &cache, priv_data_t priv_data) {
        if (handle_on_remove_object_) {
            handle_on_remove_object_(*this, key, cache, priv_data);
        }
    }

    virtual void on_evt_pull_cache(const ptr_t &cache, priv_data_t priv_data) {
        if (handle_on_pull_cache_) {
            handle_on_pull_cache_(*this, cache, priv_data);
        }
    }

    virtual void on_evt_pull_object(const ptr_t &cache, priv_data_t priv_data) {
        if (handle_on_pull_object_) {
            handle_on_pull_object_(*this, cache, priv_data);
        }
    }

private:
    remove_fn_t handle_on_remove_cache_;
    remove_fn_t handle_on_remove_object_;
    pull_fn_t handle_on_pull_cache_;
    pull_fn_t handle_on_pull_object_;

    std::unordered_map<key_t, ptr_t> caches_;
};


#endif //_ROUTER_ROUTER_MANAGER_H
