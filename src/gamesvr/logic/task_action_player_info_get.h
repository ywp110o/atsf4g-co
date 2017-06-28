//
// Created by owent on 2016/10/6.
//

#ifndef _LOGIC_ACTION_TASK_ACTION_PLAYER_INFO_GET_H
#define _LOGIC_ACTION_TASK_ACTION_PLAYER_INFO_GET_H

#pragma once

#include <dispatcher/actor_action_cs_req_base.h>

class task_action_player_info_get : public actor_action_cs_req_base {
public:
  task_action_player_info_get();
  ~task_action_player_info_get();

  virtual int operator()(hello::message_container &msg);

  virtual int on_success();
  virtual int on_failed();

private:
  hello::CSMsg &get_rsp();
  hello::CSMsg *rsp_;
};

#endif //_LOGIC_ACTION_TASK_ACTION_PLAYER_INFO_GET_H
