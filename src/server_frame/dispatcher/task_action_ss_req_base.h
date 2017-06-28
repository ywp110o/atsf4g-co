//
// Created by owt50 on 2016/9/26.
//

#ifndef _DISPATCHER_TASK_ACTION_SS_REQ_BASE_H
#define _DISPATCHER_TASK_ACTION_SS_REQ_BASE_H

#pragma once

#include "task_action_base.h"

class task_action_ss_req_base : public task_action_base {
public:
    typedef task_action_base base_type;
    typedef base_type::msg_type msg_type;
    typedef msg_type& msg_ref_type;
    typedef const msg_type& msg_cref_type;
    typedef msg_type* msg_ptr_type;

protected:
    using base_type::get_request;

public:
    using base_type::name;
    using base_type::get_task_id;
    using base_type::set_ret_code;
    using base_type::get_ret_code;
    using base_type::set_rsp_code;
    using base_type::get_rsp_code;

public:
    task_action_ss_req_base();
    virtual ~task_action_ss_req_base();

    uint64_t get_request_bus_id() const;

    const std::string& get_player_openid() const;

    std::shared_ptr<player> get_player() const;

    msg_ref_type add_rsp_msg(int32_t svr_msg_type, uint64_t dst_pd = 0);

    static int32_t init_msg(msg_ref_type msg, uint64_t dst_pd, int32_t ss_type);
    static int32_t init_msg(msg_ref_type msg, uint64_t dst_pd, int32_t ss_type, msg_cref_type req_msg);

protected:
    virtual void send_rsp_msg();

private:
    mutable std::string player_openid_;
    mutable std::shared_ptr<player> player_inst_;
    std::list<msg_type> rsp_msgs_;
};


#endif //ATF4G_CO_TASK_ACTION_SS_REQ_BASE_H
