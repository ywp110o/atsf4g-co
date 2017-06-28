//
// Created by owt50 on 2016/10/9.
//

#ifndef _RPC_DB_UUID_H
#define _RPC_DB_UUID_H

#pragma once

#include <string>

namespace rpc {
    namespace db {
        namespace uuid {
            /**
             * 生成指定类型的UUID
             * @param type ID类型
             * @param uuid 输出生成的UUID
             * @return 0或错误码
             */
            int generate(uint32_t type, std::string &uuid);
        }
    }
}

#endif //_RPC_DB_UUID_H
