//
// Created by owt50 on 2016/11/14.
//

#ifndef _DISPATCHER_ACTOR_ACRION_CS_REQ_BASE_H
#define _DISPATCHER_ACTOR_ACRION_CS_REQ_BASE_H

#pragma once

#include "actor_action_base.h"

class session;

class actor_action_cs_req_base : public actor_action_base {
public:
    typedef actor_action_base base_type;
    typedef hello::CSMsg msg_type;
    typedef msg_type &msg_ref_type;
    typedef const msg_type &msg_cref_type;
    typedef msg_type *msg_ptr_type;
    typedef const msg_type *msg_cptr_type;

protected:
    using base_type::get_request;

public:
    using base_type::name;
    using base_type::set_ret_code;
    using base_type::get_ret_code;
    using base_type::set_rsp_code;
    using base_type::get_rsp_code;

public:
    actor_action_cs_req_base();
    virtual ~actor_action_cs_req_base();

    msg_cptr_type get_cs_request() const;
    msg_ptr_type get_cs_request();

    std::pair<uint64_t, uint64_t> get_gateway_info() const;

    std::shared_ptr<session> get_session() const;

    std::shared_ptr<player> get_player() const;

    msg_ref_type add_rsp_msg();

    std::list<msg_type> &get_rsp_list();
    const std::list<msg_type> &get_rsp_list() const;

protected:
    virtual void send_rsp_msg();

private:
    mutable std::shared_ptr<session> session_inst_;
    std::list<msg_type> rsp_msgs_;
};


#endif //_DISPATCHER_ACTOR_ACRION_CS_REQ_BASE_H
