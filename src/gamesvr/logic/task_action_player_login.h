//
// Created by owent on 2016/10/6.
//

#ifndef _LOGIC_ACTION_TASK_ACTION_PLAYER_LOGIN_H
#define _LOGIC_ACTION_TASK_ACTION_PLAYER_LOGIN_H

#pragma once

#include <dispatcher/task_action_cs_req_base.h>

class task_action_player_login : public task_action_cs_req_base {
public:
    task_action_player_login();
    ~task_action_player_login();

    virtual int operator()(hello::message_container& msg);

    virtual int on_success();
    virtual int on_failed();
private:
    bool is_new_player_;
};


#endif //_LOGIC_ACTION_TASK_ACTION_PLAYER_LOGIN_H
