//
// Created by owent on 2016/10/6.
//

#ifndef _LOGIC_ACTION_TASK_ACTION_PLAYER_LOGOUT_H
#define _LOGIC_ACTION_TASK_ACTION_PLAYER_LOGOUT_H

#pragma once

#include <dispatcher/task_action_no_req_base.h>

class task_action_player_logout: public task_action_no_req_base {
public:
    task_action_player_logout(uint64_t bus_id, uint64_t session_id);
    ~task_action_player_logout();

    virtual int operator()(hello::message_container& msg);

    virtual int on_success();
    virtual int on_failed();

private:
    uint64_t atgateway_bus_id_;
    uint64_t atgateway_session_id_;
};


#endif //_LOGIC_ACTION_TASK_ACTION_PLAYER_LOGOUT_H
