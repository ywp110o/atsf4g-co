//
// Created by owent on 2016/10/6.
//

#ifndef _LOGIC_ACTION_TASK_ACTION_AUTO_SAVE_PLAYERS_H
#define _LOGIC_ACTION_TASK_ACTION_AUTO_SAVE_PLAYERS_H

#pragma once

#include <dispatcher/task_action_no_req_base.h>

class task_action_auto_save_players : public task_action_no_req_base {
public:
    task_action_auto_save_players();
    ~task_action_auto_save_players();

    virtual int operator()(hello::message_container& msg);

    virtual int on_success();
    virtual int on_failed();
    virtual int on_timeout();
private:
    int success_count_;
    int failed_count_;
};


#endif //_LOGIC_ACTION_TASK_ACTION_AUTO_SAVE_PLAYERS_H
