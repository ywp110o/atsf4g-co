//
// Created by owent on 2016/10/4.
//

#ifndef _RPC_RPC_UTILS_H
#define _RPC_RPC_UTILS_H

#pragma once

#include <stdint.h>
#include <cstddef>
#include <string>
#include <vector>

namespace hello {
    class message_container;
}

namespace rpc {
    int wait(hello::message_container& msgc);
}

#endif //_RPC_RPC_UTILS_H
