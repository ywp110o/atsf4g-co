
#include "config/compiler_features.h"
#include "proto_impl.h"

#ifndef ATBUS_MACRO_TLS_MERGE_BUFFER_LEN

#if defined(ATBUS_MACRO_MSG_LIMIT) && defined(ATBUS_MACRO_DATA_ALIGN_TYPE)
#define ATBUS_MACRO_TLS_MERGE_BUFFER_LEN (ATBUS_MACRO_MSG_LIMIT - sizeof(ATBUS_MACRO_DATA_ALIGN_TYPE))
#else
#define ATBUS_MACRO_TLS_MERGE_BUFFER_LEN 65536
#endif
#endif

#if defined(UTIL_CONFIG_THREAD_LOCAL)
namespace atframe {
    namespace gateway {
        namespace detail {
            static char *atgateway_get_msg_buffer(::atframe::gateway::proto_base::tls_buffer_t::type t) {
                static UTIL_CONFIG_THREAD_LOCAL char ret[ ::atframe::gateway::proto_base::tls_buffer_t::EN_TBT_MAX]
                                                        [ATBUS_MACRO_TLS_MERGE_BUFFER_LEN];
                return ret[t];
            }
        }
    }
}
#else

#include <pthread.h>
namespace atframe {
    namespace gateway {
        namespace detail {
            static pthread_once_t gt_atgateway_get_msg_buffer_tls_once = PTHREAD_ONCE_INIT;
            static pthread_key_t gt_atgateway_get_msg_buffer_tls_key[ ::atframe::gateway::proto_base::tls_buffer_t::EN_TBT_MAX];

            static void dtor_pthread_atgateway_get_msg_buffer_tls(void *p) {
                char *res = reinterpret_cast<char *>(p);
                if (NULL != res) {
                    delete[] res;
                }
            }

            static void init_pthread_atgateway_get_msg_buffer_tls() {
                for (int i = 0; i < ::atframe::gateway::proto_base::tls_buffer_t::EN_TBT_MAX; ++i) {
                    (void)pthread_key_create(&gt_atgateway_get_msg_buffer_tls_key[i], dtor_pthread_atgateway_get_msg_buffer_tls);
                }
            }

            static char *atgateway_get_msg_buffer(::atframe::gateway::proto_base::tls_buffer_t::type t) {
                (void)pthread_once(&gt_atgateway_get_msg_buffer_tls_once, init_pthread_atgateway_get_msg_buffer_tls);
                char *ret = reinterpret_cast<char *>(pthread_getspecific(gt_atgateway_get_msg_buffer_tls_key[i]));
                if (NULL == ret) {
                    ret = new char[ATBUS_MACRO_TLS_MERGE_BUFFER_LEN];
                    pthread_setspecific(gt_atgateway_get_msg_buffer_tls_key[i], ret);
                }
                return ret;
            }
        }
    }
}

#endif

namespace atframe {
    namespace gateway {
        proto_base::proto_base() : flags_(0), callbacks_(NULL), private_data_(NULL) {}
        proto_base::~proto_base(){};

        void *proto_base::get_tls_buffer(tls_buffer_t::type tls_type) {
            return ::atframe::gateway::detail::atgateway_get_msg_buffer(tls_type);
        }

        size_t proto_base::get_tls_length(tls_buffer_t::type tls_type) { return ATBUS_MACRO_TLS_MERGE_BUFFER_LEN; }

        int proto_base::write_done(int status) {
            if (0 == (flags_ & flag_t::EN_PFT_WRITING)) {
                return 0;
            }

            flags_ &= ~flag_t::EN_PFT_WRITING;
            return 0;
        }

        int proto_base::close(int reason) { return 0; }

        bool proto_base::check_reconnect(proto_base *other) { return false; }
    }
}