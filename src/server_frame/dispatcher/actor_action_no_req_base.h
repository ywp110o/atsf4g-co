//
// Created by owt50 on 2016/11/14.
//

#ifndef _DISPATCHER_ACTOR_ACTION_NO_REQ_BASE_H
#define _DISPATCHER_ACTOR_ACTION_NO_REQ_BASE_H

#pragma once

#include "actor_action_base.h"

class actor_action_no_req_base : public actor_action_base {
public:
    typedef actor_action_base base_type;

protected:
    using base_type::get_request;

public:
    using base_type::name;
    using base_type::set_ret_code;
    using base_type::get_ret_code;
    using base_type::set_rsp_code;
    using base_type::get_rsp_code;

public:
    actor_action_no_req_base();
    ~actor_action_no_req_base();

    std::shared_ptr<player> get_player() const;

protected:
    virtual void send_rsp_msg();
};


#endif //_DISPATCHER_ACTOR_ACTION_NO_REQ_BASE_H
