#ifndef _ATFRAME_SERVICE_ATGATEWAY_SOCK_SESSION_H_
#define _ATFRAME_SERVICE_ATGATEWAY_SOCK_SESSION_H_

#pragma once

#include "session_base.h"
#include <std/smart_ptr.h>

namespace atframe {
    namespace gateway {
        class sock_session : public session_base {
        public:
            typedef session_base::id_t id_t;
            typedef session_base::limit_t limit_t;
            typedef std::shared_ptr<libuv_session> ptr_t;

            typedef fd_t sock_t;

        public:
            static ptr_t create(sock_t);

        private:
            sock_t sock_;
        };
    }
}

#endif