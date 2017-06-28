//
// Created by owt50 on 2016/9/26.
//

#ifndef _DISPATCHER_TASK_MANAGER_H
#define _DISPATCHER_TASK_MANAGER_H

#pragma once

#include <design_pattern/singleton.h>
#include <libcotask/task.h>
#include <libcotask/task_manager.h>
#include <libcopp/stack/stack_allocator.h>

#include <utility/environment_helper.h>

namespace hello {
    class message_container;
}

class actor_action_base;

/**
 * @brief 协程任务和简单actor的管理创建manager类
 * @note 涉及异步处理的任务全部走协程任务，不涉及异步调用的模块可以直接使用actor。
 *       actor会比task少一次栈初始化开销（大约8us的CPU+栈所占用的内存）,在量大但是无异步调用的模块（比如地图同步行为）可以节省CPU和内存
 */
class task_manager : public ::util::design_pattern::singleton<task_manager> {
public:
    struct task_macro_coroutine {
        typedef copp::detail::coroutine_context_base coroutine_t;
        typedef copp::allocator::default_statck_allocator stack_allocator_t; // TODO 有需要可以换成通过内存池创建栈空间
        typedef copp::detail::coroutine_context_container<coroutine_t, stack_allocator_t> coroutine_container_t;
    };

    typedef cotask::task<task_macro_coroutine> task_t; // TODO 以后有需要可以换成通过内存池创建协程任务
    typedef typename task_t::id_t id_t;
    typedef typename task_t::action_ptr_t task_action_ptr_t;
    typedef std::function<int(task_manager::id_t &)> task_action_creator_t;

    typedef std::shared_ptr<actor_action_base> actor_action_ptr_t;
    typedef std::function<actor_action_ptr_t()> actor_action_creator_t;

    template<typename TAction>
    struct task_action_maker_t {
        int operator()(task_manager::id_t &task_id) {
            return task_manager::me()->create_task<TAction>(task_id);
        };
    };

    template<typename TAction>
    struct actor_action_maker_t {
        actor_action_ptr_t operator()() {
            return task_manager::me()->create_actor<TAction>();
        };
    };

protected:
    task_manager();
    ~task_manager();

public:
    int init();

    /**
     * 获取栈大小
     */
    size_t get_stack_size() const;

    /**
     * @brief 创建任务
     * @param task_id 协程任务的ID
     * @param args 传入构造函数的参数
     * @return 0或错误码
     */
    template <typename TAction, typename... TParams>
    int create_task(id_t &task_id, TParams &&... args) {
        std::shared_ptr<task_t> res = task_t::create_with<TAction>(get_stack_size(), std::forward<TParams>(args)...);
        if (!res) {
            task_id = 0;
            return report_create_error(__FUNCTION__);
        }

        task_id = res->get_id();
        return add_task(res, 0);
    }

    /**
     * @brief 创建任务并指定超时时间
     * @param task_id 协程任务的ID
     * @param timeout 超时时间
     * @param args 传入构造函数的参数
     * @return 0或错误码
     */
    template <typename TAction, typename... TParams>
    int create_task_with_timeout(id_t &task_id, time_t timeout, TParams &&... args) {
        std::shared_ptr<task_t> res = task_t::create_with<TAction>(get_stack_size(), std::forward<TParams>(args)...);
        if (!res) {
            task_id = 0;
            return report_create_error(__FUNCTION__);
        }

        task_id = res->get_id();
        return add_task(res, timeout);
    }

    /**
     * @brief 创建协程任务构造器
     * @return 任务构造器
     */
    template <typename TAction>
    task_action_creator_t make_task_creator() {
        return task_action_maker_t<TAction>();
    }

    /**
     * @brief 开始任务
     * @param task_id 协程任务的ID
     * @param msg 相关消息体
     * @return 0或错误码
     */
    int start_task(id_t task_id, hello::message_container &msg);

    /**
     * @brief 恢复任务
     * @param task_id 协程任务的ID
     * @param msg 相关消息体
     * @return 0或错误码
     */
    int resume_task(id_t task_id, hello::message_container &msg);

    /**
     * @brief 创建Actor
     * @note 所有的actor必须使用组合的方式执行，不允许使用协程RPC操作
     * @param args 传入构造函数的参数
     * @return 0或错误码
     */
    template <typename TActor, typename... TParams>
    std::shared_ptr<TActor> create_actor(TParams &&... args) {
        return std::make_shared<TActor>(std::forward<TParams>(args)...);
    }

    /**
     * @brief 创建Actor构造器
     * @return Actor构造器
     */
    template <typename TAction>
    actor_action_creator_t make_actor_creator() {
        return actor_action_maker_t<TAction>();
    }

    /**
     * @brief tick，可能会触发任务过期
     */
    int tick(time_t sec, int nsec);

private:
    /**
     * @brief 创建任务
     * @param task 协程任务
     * @param timeout 超时时间
     * @return 0或错误码
     */
    int add_task(const std::shared_ptr<task_t> &task, time_t timeout = 0);

    int report_create_error(const char* fn_name);
private:
    typedef UTIL_ENV_AUTO_MAP(id_t, cotask::task_mgr_node<id_t>) native_task_container_t;
    typedef cotask::task_manager<id_t,  native_task_container_t> mgr_t;
    typedef typename mgr_t::ptr_t mgr_ptr_t;
    mgr_ptr_t native_mgr_;
};


#endif //ATF4G_CO_TASK_MANAGER_H
