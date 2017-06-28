//
// Created by owent on 2016/10/6.
//

#ifndef _LOGIC_ACTION_TASK_ACTION_PING_H
#define _LOGIC_ACTION_TASK_ACTION_PING_H

#pragma once

#include <dispatcher/task_action_cs_req_base.h>

class task_action_ping : public task_action_cs_req_base {
public:
    task_action_ping();
    ~task_action_ping();

    virtual int operator()(hello::message_container& msg);

    virtual int on_success();
    virtual int on_failed();
};


#endif //_LOGIC_ACTION_TASK_ACTION_PING_H
