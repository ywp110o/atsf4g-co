//
// Created by owent on 2016/10/6.
//

#ifndef LOGIC_ACTION_TASK_ACTION_AUTO_SAVE_PLAYERS_H
#define LOGIC_ACTION_TASK_ACTION_AUTO_SAVE_PLAYERS_H

#pragma once

#include <dispatcher/task_action_no_req_base.h>

class task_action_auto_save_players : public task_action_no_req_base {
public:
    struct ctor_param_t {};

public:
    using task_action_no_req_base::operator();

public:
    task_action_auto_save_players(ctor_param_t COPP_MACRO_RV_REF param);
    ~task_action_auto_save_players();

    virtual int operator()();

    virtual int on_success();
    virtual int on_failed();
    virtual int on_timeout();

private:
    int success_count_;
    int failed_count_;
    ctor_param_t ctor_param_;
};


#endif //_LOGIC_ACTION_TASK_ACTION_AUTO_SAVE_PLAYERS_H
