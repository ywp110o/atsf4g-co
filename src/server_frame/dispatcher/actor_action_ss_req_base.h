//
// Created by owt50 on 2016/11/14.
//

#ifndef DISPATCHER_ACTOR_ACTION_SS_REQ_BASE_H
#define DISPATCHER_ACTOR_ACTION_SS_REQ_BASE_H

#pragma once

#include "dispatcher_type_defines.h"

#include "actor_action_base.h"

#include <protocol/pbdesc/svr.protocol.pb.h>

class actor_action_ss_req_base : public actor_action_req_base<hello::SSMsg> {
public:
    typedef actor_action_req_base<hello::SSMsg> base_type;
    typedef base_type::msg_type msg_type;
    typedef msg_type &msg_ref_type;
    typedef const msg_type &msg_cref_type;

protected:
    using base_type::get_request;

public:
    using base_type::get_ret_code;
    using base_type::get_rsp_code;
    using base_type::name;
    using base_type::set_ret_code;
    using base_type::set_rsp_code;
    using base_type::operator();

public:
    actor_action_ss_req_base(dispatcher_start_data_t COPP_MACRO_RV_REF start_param);
    virtual ~actor_action_ss_req_base();

    uint64_t get_request_bus_id() const;

    msg_ref_type add_rsp_msg(uint64_t dst_pd = 0);

    static int32_t init_msg(msg_ref_type msg, uint64_t dst_pd);
    static int32_t init_msg(msg_ref_type msg, uint64_t dst_pd, msg_cref_type req_msg);

protected:
    virtual void send_rsp_msg();

private:
    std::list<msg_type> rsp_msgs_;
};


#endif //_DISPATCHER_ACTOR_ACTION_SS_REQ_BASE_H
