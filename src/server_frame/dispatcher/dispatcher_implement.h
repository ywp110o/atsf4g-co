//
// Created by owt50 on 2016/9/26.
//

#ifndef _DISPATCHER_DISPATCHER_IMPLEMENT_H
#define _DISPATCHER_DISPATCHER_IMPLEMENT_H

#pragma once

#include <list>

#include <atframe/atapp_module_impl.h>

#include <utility/environment_helper.h>

#include "task_manager.h"

namespace hello {
    class message_container;
}

class dispatcher_implement : public ::atapp::module_impl {
public:
    typedef uint32_t msg_type_t;
    typedef hello::message_container* msg_ptr_t;
    typedef UTIL_ENV_AUTO_MAP(msg_type_t, task_manager::task_action_creator_t) msg_task_action_set_t;
    typedef UTIL_ENV_AUTO_MAP(msg_type_t, task_manager::actor_action_creator_t) msg_actor_action_set_t;

    struct msg_filter_data_t {
        const void* msg_buffer;
        size_t msg_size;

        msg_ptr_t msg;
    };

    /**
     * @brief 消息过滤器函数，调用式为: bool(const msg_filter_data_t&)
     * @note 返回false可以中断后续过滤器的执行并禁止消息分发
     */
    typedef std::function<bool(const msg_filter_data_t&)> msg_filter_handle_t;
public:
    virtual int init();

    /**
     * @brief 接收消息回调接口，通常会尝试恢复协程任务运行或创建一个协程任务
     * @param msgc 消息数据包装器
     * @param msg_buf 数据地址
     * @param msg_size 数据长度
     * @return 返回错误码或0
     */
    virtual int32_t on_recv_msg(msg_ptr_t msgc, const void* msg_buf, size_t msg_size);

    /**
     * @brief 数据解包
     * @param msg_container 填充目标
     * @param msg_buf 数据地址
     * @param msg_size 数据长度
     * @return 返回错误码或0
     */
    virtual int32_t unpack_msg(msg_ptr_t msg_container, const void* msg_buf, size_t msg_size) = 0;

    /**
     * @brief 获取任务信息
     * @param msg_container 填充目标
     * @return 相关的任务id
     */
    virtual uint64_t pick_msg_task(const msg_ptr_t msg_container);

    /**
     * @brief 创建协程任务
     * @param msg_container 填充目标
     * @param task_id 相关的任务id
     * @return 返回错误码或0
     */
    virtual int create_task(const msg_ptr_t msg_container, task_manager::id_t& task_id);

    /**
     * @brief 创建Actor
     * @param msg_container 填充目标
     * @return 返回错误码或0
     */
    virtual task_manager::actor_action_ptr_t create_actor(const msg_ptr_t msg_container);

    /**
     * @brief 获取消息名称
     * @param msg_container 填充目标
     * @return 消息名称
     */
    virtual const std::string& pick_msg_name(const msg_ptr_t msg_container) = 0;

    /**
     * @brief 获取消息名称
     * @param msg_container 填充目标
     * @return 消息类型ID
     */
    virtual msg_type_t pick_msg_type_id(const msg_ptr_t msg_container) = 0;

    /**
     * @brief 获取消息名称到ID的映射
     * @param msg_name 消息名称
     * @return 消息类型ID
     */
    virtual msg_type_t msg_name_to_type_id(const std::string& msg_name) = 0;

    /**
     * @brief 注册Action
     * @param msg_name action名称
     * @return 或错误码
     */
    template <typename TAction>
    int register_action(const std::string& msg_name) {
        return _register_action(msg_name, task_manager::me()->make_task_creator<TAction>());
    }

    /**
     * @brief 注册Action
     * @param msg_name action名称
     * @return 或错误码
     */
    template <typename TAction>
    int register_actor(const std::string& msg_name) {
        return _register_action(msg_name, task_manager::me()->make_actor_creator<TAction>());
    }

    /**
     * @brief 添加前置过滤器
     * @param fn 函数或仿函数
     * @note 被添加的过滤器会先执行
     */
    void push_filter_to_front(msg_filter_handle_t fn);

    /**
     * @brief 添加后置过滤器
     * @param fn 函数或仿函数
     * @note 被添加的过滤器会最后执行
     */
    void push_filter_to_back(msg_filter_handle_t fn);

protected:
    const std::string& get_empty_string();

private:

    int _register_action(const std::string& msg_name, task_manager::task_action_creator_t action);
    int _register_action(const std::string& msg_name, task_manager::actor_action_creator_t action);

private:
    msg_task_action_set_t task_action_name_map_;
    msg_actor_action_set_t actor_action_name_map_;
    std::list<msg_filter_handle_t> msg_filter_list_;
};


#define REG_TASK_MSG_HANDLE(dispatcher, ret, act, proto)\
    if (ret < 0) { dispatcher::me()->register_action<act>(proto); } \
    else { ret = dispatcher::me()->register_action<act>(proto); }

#define REG_ACTOR_MSG_HANDLE(dispatcher, ret, act, proto)\
    if (ret < 0) { dispatcher::me()->register_actor<act>(proto); } \
    else { ret = dispatcher::me()->register_actor<act>(proto); }

#endif //ATF4G_CO_DISPATCHER_IMPLEMENT_H
