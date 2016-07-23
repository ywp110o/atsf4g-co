#include "session_manager.h"

namespace atframe {
    namespace gateway {
        int session_manager::init(uv_loop_t *evloop) {
            evloop_ = evloop;
            return 0;
        }
    }
}