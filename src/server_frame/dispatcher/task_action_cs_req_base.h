//
// Created by owt50 on 2016/9/26.
//

#ifndef DISPATCHER_TASK_ACTION_CS_REQ_BASE_H
#define DISPATCHER_TASK_ACTION_CS_REQ_BASE_H

#pragma once

#include "dispatcher_type_defines.h"

#include "task_action_base.h"

#include <protocol/pbdesc/com.protocol.pb.h>

class session;

class task_action_cs_req_base : public task_action_req_base<hello::CSMsg> {
public:
    typedef task_action_req_base<hello::CSMsg> base_type;
    typedef base_type::msg_type msg_type;
    typedef msg_type &msg_ref_type;
    typedef const msg_type &msg_cref_type;

protected:
    using base_type::get_request;

public:
    using base_type::get_ret_code;
    using base_type::get_rsp_code;
    using base_type::get_task_id;
    using base_type::name;
    using base_type::set_ret_code;
    using base_type::set_rsp_code;
    using base_type::operator();

public:
    task_action_cs_req_base(dispatcher_start_data_t COPP_MACRO_RV_REF start_param);
    virtual ~task_action_cs_req_base();

    std::pair<uint64_t, uint64_t> get_gateway_info() const;

    std::shared_ptr<session> get_session() const;

    msg_ref_type add_rsp_msg();

    std::list<msg_type> &get_rsp_list();
    const std::list<msg_type> &get_rsp_list() const;

protected:
    virtual void send_rsp_msg();

private:
    mutable std::shared_ptr<session> session_inst_;
    std::list<msg_type> rsp_msgs_;
};


#endif // ATF4G_CO_TASK_ACTION_CS_REQ_BASE_H
