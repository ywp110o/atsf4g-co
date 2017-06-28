//
// Created by owt50 on 2016/9/28.
//

#ifndef _RPC_DB_PLAYER_H
#define _RPC_DB_PLAYER_H

#pragma once

#include <stdint.h>
#include <cstddef>
#include <string>
#include <vector>

#include <protocol/pbdesc/svr.container.pb.h>

namespace rpc {
    namespace db {
        namespace player {
            /**
             * @brief 获取用户表所有数据的rpc操作
             * @param openid 登入用户的openid
             * @param rsp 返回的登入信息
             * @return 0或错误码
             */
            int get_all(const char* openid, hello::table_user& rsp, std::string &version);

            /**
             * @brief 设置用户表的rpc操作
             * @param openid 登入用户的openid
             * @param store 要保持的数据
             * @warning 默认值会被忽略，比如空message或者空字符串，或者0不会更新
             * @return 0或错误码
             */
            int set(const char* openid, hello::table_user& store, std::string &version);

        }
    }
}

#endif //_RPC_DB_PLAYER_H
