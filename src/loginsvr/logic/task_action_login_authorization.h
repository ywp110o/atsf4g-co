//
// Created by owent on 2016/10/6.
//

#ifndef _LOGIC_ACTION_TASK_ACTION_LOGIN_AUTHORIZATION_H
#define _LOGIC_ACTION_TASK_ACTION_LOGIN_AUTHORIZATION_H

#pragma once

#include <utility/environment_helper.h>

#include <dispatcher/task_action_cs_req_base.h>

class task_action_login_authorization : public task_action_cs_req_base {
public:
    typedef int (task_action_login_authorization::*auth_fn_t)(const ::hello::CSLoginAuthReq &);

public:
    task_action_login_authorization();
    ~task_action_login_authorization();

    virtual int operator()(hello::message_container &msg);

    virtual int on_success();
    virtual int on_failed();

private:
    int32_t check_proto_update(uint32_t iVer);

    auth_fn_t get_verify_fn(uint32_t plat_id);

    void init_login_data(hello::table_login &tb, const ::hello::CSLoginAuthReq &req, const std::string &uuid, uint32_t channel_id);

    /**
     * @brief openid加上平台号
     * @param req 请求包
     * @return 最终openid
     */
    std::string make_openid(const hello::CSLoginAuthReq &req);

    /**
     * @brief 校验本地账户
     * @param req 请求包
     * @return 回包的错误码或0
     */
    int verify_plat_account(const ::hello::CSLoginAuthReq &req);

private:
    bool is_new_player_;
    uint32_t strategy_type_;
    std::string version_;
    std::string final_openid_;
    hello::table_login login_data_;
    hello::DClientUpdateCfg update_info_; // 更新信息

    google::protobuf::RepeatedPtrField< ::google::protobuf::string> gamesvr_addrs_; // 登录地址

    static UTIL_ENV_AUTO_SET(std::string) white_skip_openids_;
};


#endif //_LOGIC_ACTION_TASK_ACTION_LOGIN_AUTHORIZATION_H
