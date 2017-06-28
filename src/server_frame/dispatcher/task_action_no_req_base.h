//
// Created by owt50 on 2016/9/26.
//

#ifndef _DISPATCHER_TASK_ACTION_NO_REQ_BASE_H
#define _DISPATCHER_TASK_ACTION_NO_REQ_BASE_H

#pragma once

#include "task_action_base.h"

class task_action_no_req_base : public task_action_base  {
public:
    typedef task_action_base base_type;

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
    task_action_no_req_base();
    ~task_action_no_req_base();

    std::shared_ptr<player> get_player() const;

protected:
    virtual void send_rsp_msg();
};


#endif //ATF4G_CO_TASK_ACTION_NO_REQ_BASE_H
