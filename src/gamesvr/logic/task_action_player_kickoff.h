//
// Created by owent on 2016/10/6.
//

#ifndef _LOGIC_ACTION_TASK_ACTION_PLAYER_KICKOFF_H
#define _LOGIC_ACTION_TASK_ACTION_PLAYER_KICKOFF_H

#pragma once

#include <dispatcher/task_action_ss_req_base.h>

class task_action_player_kickoff : public task_action_ss_req_base {
public:
    task_action_player_kickoff();
    ~task_action_player_kickoff();

    virtual int operator()(hello::message_container& msg);

    virtual int on_success();
    virtual int on_failed();
};


#endif //_LOGIC_ACTION_TASK_ACTION_PLAYER_KICKOFF_H
