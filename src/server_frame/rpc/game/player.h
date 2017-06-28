//
// Created by owt50 on 2016/9/28.
//

#ifndef _RPC_GAME_PLAYER_H
#define _RPC_GAME_PLAYER_H

#pragma once

#include <stdint.h>
#include <cstddef>
#include <string>
#include <vector>

namespace rpc {
    namespace game {
        namespace player {
            /**
             * @brief kickoff RPC
             * @param dst_bus_id server bus id
             * @param openid player's openid
             * @param reason kickoff reason
             * @return 0 or error code
             */
            int send_kickoff(uint64_t dst_bus_id, const std::string &openid, int32_t reason = 0);

        }
    }
}


#endif //_RPC_GAME_PLAYER_H
