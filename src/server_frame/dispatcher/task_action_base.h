//
// Created by owt50 on 2016/9/26.
//

#ifndef _DISPATCHER_TASK_ACTION_BASE_H
#define _DISPATCHER_TASK_ACTION_BASE_H

#pragma once

#include <list>

#include <std/smart_ptr.h>

#include <libcotask/task.h>

#include <protocol/pbdesc/svr.container.pb.h>

/**
 * action 默认结构
 * action rpc 接口
 * action rpc.1 记录+Dispatcher发送接口（出错则直接返回）
 * action rpc.2 检查回包+填充rsp包+返回调用者
 *
 * action rpc 启动（填充初始包+ operator()(void*) => operator()(message_container) ）
 */

class player;

class task_action_base : public ::cotask::impl::task_action_impl {
public:
    typedef hello::message_container msg_type;

protected:
    task_action_base();
    virtual ~task_action_base();

public:
    virtual const char *name() const;

    int operator()(void *priv_data);
    virtual int hook_run();

    virtual int operator()(hello::message_container &msg) = 0;
    virtual std::shared_ptr<player> get_player() const = 0;

    virtual int on_success();
    virtual int on_failed();
    virtual int on_timeout();

    uint64_t get_task_id() const;
    unsigned long long get_task_id_llu() const;

protected:
    hello::message_container &get_request();
    const hello::message_container &get_request() const;
    virtual void send_rsp_msg() = 0;

public:
    /**
     * @brief 获取逻辑返回码
     * @note 默认值为 T_APP_SUCCESS
     * @see T_APP_SUCCESS
     * @return 返回码
     */
    inline int32_t get_ret_code() const { return ret_code_; }

    /**
     * @brief 获取回包返回码
     * @note 默认值为 Polar::EN_CS_SUCCESS
     * @see Polar::EN_CS_SUCCESS
     * @return 回包返回码
     */
    inline int32_t get_rsp_code() const { return rsp_code_; }

protected:
    /**
     * @brief 设置逻辑返回码
     * @note 用于临时存储逻辑操作错误码
     * @param iRetCode 返回码
     */
    inline void set_ret_code(int32_t ret_code) { ret_code_ = ret_code; }

    /**
     * @brief 设置回包返回码
     * @note 用于临时存储回包返回码
     * @param iRetCode 回包返回码
     */
    inline void set_rsp_code(int32_t rsp_code) { rsp_code_ = rsp_code; }

    /**
     * @brief 禁用结束事件响应
     */
    inline void disable_finish_evt() { evt_disabled_ = false; }

    /**
     * @brief 禁用自动回包
     */
    inline void disable_rsp_msg() { rsp_msg_disabled_ = false; }

private:
    uint64_t task_id_;
    hello::message_container request_msg_;
    int32_t ret_code_;
    int32_t rsp_code_;
    bool rsp_msg_disabled_;
    bool evt_disabled_;
};


#endif // ATF4G_CO_TASK_ACTION_BASE_H
