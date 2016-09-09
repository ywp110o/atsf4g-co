#include "libatgw_inner_v1_c.h"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>


#include "common/string_oprs.h"

#include <config/atframe_services_build_feature.h>
#include <inner_v1/libatgw_proto_inner.h>


struct g_libatgw_inner_v1_c_callbacks_t {
    ::atframe::gateway::proto_base::proto_callbacks_t callbacks;
    libatgw_inner_v1_c_on_write_start_fn_t write_start_fn;
    libatgw_inner_v1_c_on_message_fn_t on_message_fn;
    libatgw_inner_v1_c_on_init_new_session_fn_t on_init_new_session_fn;
    libatgw_inner_v1_c_on_init_reconnect_fn_t on_init_reconnect_fn;
    libatgw_inner_v1_c_on_close_fn_t on_close_fn;
    libatgw_inner_v1_c_on_handshake_done_fn_t on_handshake_done_fn;
    libatgw_inner_v1_c_on_handshake_done_fn_t on_handshake_update_fn;
    libatgw_inner_v1_c_on_error_fn_t on_error_fn;

    g_libatgw_inner_v1_c_callbacks_t()
        : write_start_fn(NULL), on_message_fn(NULL), on_init_new_session_fn(NULL), on_init_reconnect_fn(NULL), on_close_fn(NULL), on_handshake_done_fn(NULL),
          on_error_fn(NULL) {}
};

static g_libatgw_inner_v1_c_callbacks_t *libatgw_inner_v1_c_get_c_callbacks() {
    static g_libatgw_inner_v1_c_callbacks_t cbks;
    return &cbks;
}

static int32_t proto_inner_callback_on_write(::atframe::gateway::proto_base *proto, void *buffer, size_t sz, bool *is_done) {
    if (NULL == buffer || 0 == sz) {
        if (NULL != is_done) {
            *is_done = true;
        }
        return ::atframe::gateway::error_code_t::EN_ECT_PARAM;
    }

    libatgw_inner_v1_c_on_write_start_fn_t fn = libatgw_inner_v1_c_get_c_callbacks()->write_start_fn;
    if (NULL == fn) {
        if (NULL != is_done) {
            *is_done = true;
        }
        return ::atframe::gateway::error_code_t::EN_ECT_MISS_CALLBACKS;
    }

    int32_t is_done_i = 0;
    int32_t ret = fn(proto, buffer, sz, &is_done_i);
    if (NULL != is_done) {
        *is_done = !!is_done_i;
    }

    return ret;
}

static int32_t proto_inner_callback_on_message(::atframe::gateway::proto_base *proto, const void *buffer, size_t sz) {
    libatgw_inner_v1_c_on_message_fn_t fn = libatgw_inner_v1_c_get_c_callbacks()->on_message_fn;
    if (NULL != fn) {
        fn(proto, buffer, sz);
    }
    return 0;
}

// useless
static int32_t proto_inner_callback_on_new_session(::atframe::gateway::proto_base *proto, uint64_t &sess_id) {
    libatgw_inner_v1_c_on_init_new_session_fn_t fn = libatgw_inner_v1_c_get_c_callbacks()->on_init_new_session_fn;
    if (NULL != fn) {
        fn(proto, &sess_id);
    }

    return 0;
}

// useless
static int32_t proto_inner_callback_on_reconnect(::atframe::gateway::proto_base *proto, uint64_t sess_id) {
    libatgw_inner_v1_c_on_init_reconnect_fn_t fn = libatgw_inner_v1_c_get_c_callbacks()->on_init_reconnect_fn;
    if (NULL != fn) {
        fn(proto, sess_id);
    }

    return 0;
}


static int32_t proto_inner_callback_on_close(::atframe::gateway::proto_base *proto, int32_t reason) {
    libatgw_inner_v1_c_on_close_fn_t fn = libatgw_inner_v1_c_get_c_callbacks()->on_close_fn;
    if (NULL != fn) {
        fn(proto, reason);
    }

    return 0;
}

static int32_t proto_inner_callback_on_handshake(::atframe::gateway::proto_base *proto, int32_t status) {
    libatgw_inner_v1_c_on_handshake_done_fn_t fn = libatgw_inner_v1_c_get_c_callbacks()->on_handshake_done_fn;
    if (NULL != fn) {
        fn(proto, status);
    }

    return 0;
}

static int32_t proto_inner_callback_on_handshake_update(::atframe::gateway::proto_base *proto, int32_t status) {
    libatgw_inner_v1_c_on_handshake_done_fn_t fn = libatgw_inner_v1_c_get_c_callbacks()->on_handshake_update_fn;
    if (NULL != fn) {
        fn(proto, status);
    }

    return 0;
}

static int32_t proto_inner_callback_on_error(::atframe::gateway::proto_base *proto, const char *filename, int32_t line, int32_t errcode, const char *errmsg) {
    libatgw_inner_v1_c_on_error_fn_t fn = libatgw_inner_v1_c_get_c_callbacks()->on_error_fn;
    if (NULL != fn) {
        fn(proto, filename, line, errcode, errmsg);
    }

    return 0;
}

static ::atframe::gateway::proto_base::proto_callbacks_t *libatgw_inner_v1_c_get_proto_callbacks() {
    g_libatgw_inner_v1_c_callbacks_t *cbks = libatgw_inner_v1_c_get_c_callbacks();

    // init
    if (!cbks->callbacks.write_fn) {
        cbks->callbacks.write_fn = proto_inner_callback_on_write;
        cbks->callbacks.message_fn = proto_inner_callback_on_message;
        cbks->callbacks.new_session_fn = proto_inner_callback_on_new_session;
        cbks->callbacks.reconnect_fn = proto_inner_callback_on_reconnect;
        cbks->callbacks.close_fn = proto_inner_callback_on_close;
        cbks->callbacks.on_handshake_done_fn = proto_inner_callback_on_handshake;
        cbks->callbacks.on_handshake_update_fn = proto_inner_callback_on_handshake_update;
        cbks->callbacks.on_error_fn = proto_inner_callback_on_error;
    }
    return &cbks->callbacks;
}

#ifdef __cplusplus
extern "C" {
#endif

ATFRAME_SYMBOL_EXPORT void __cdecl libatgw_inner_v1_c_gset_on_write_start_fn(libatgw_inner_v1_c_on_write_start_fn_t fn) {
    libatgw_inner_v1_c_get_c_callbacks()->write_start_fn = fn;
}

ATFRAME_SYMBOL_EXPORT void __cdecl libatgw_inner_v1_c_gset_on_message_fn(libatgw_inner_v1_c_on_message_fn_t fn) {
    libatgw_inner_v1_c_get_c_callbacks()->on_message_fn = fn;
}

ATFRAME_SYMBOL_EXPORT void __cdecl libatgw_inner_v1_c_gset_on_init_new_session_fn(libatgw_inner_v1_c_on_init_new_session_fn_t fn) {
    libatgw_inner_v1_c_get_c_callbacks()->on_init_new_session_fn = fn;
}

ATFRAME_SYMBOL_EXPORT void __cdecl libatgw_inner_v1_c_gset_on_init_reconnect_fn(libatgw_inner_v1_c_on_init_reconnect_fn_t fn) {
    libatgw_inner_v1_c_get_c_callbacks()->on_init_reconnect_fn = fn;
}

ATFRAME_SYMBOL_EXPORT void __cdecl libatgw_inner_v1_c_gset_on_close_fn(libatgw_inner_v1_c_on_close_fn_t fn) {
    libatgw_inner_v1_c_get_c_callbacks()->on_close_fn = fn;
}

ATFRAME_SYMBOL_EXPORT void __cdecl libatgw_inner_v1_c_gset_on_handshake_done_fn(libatgw_inner_v1_c_on_handshake_done_fn_t fn) {
    libatgw_inner_v1_c_get_c_callbacks()->on_handshake_done_fn = fn;
}

ATFRAME_SYMBOL_EXPORT void __cdecl libatgw_inner_v1_c_gset_on_handshake_update_fn(libatgw_inner_v1_c_on_handshake_done_fn_t fn) {
    libatgw_inner_v1_c_get_c_callbacks()->on_handshake_update_fn = fn;
}

ATFRAME_SYMBOL_EXPORT void __cdecl libatgw_inner_v1_c_gset_on_error_fn(libatgw_inner_v1_c_on_error_fn_t fn) {
    libatgw_inner_v1_c_get_c_callbacks()->on_error_fn = fn;
}

ATFRAME_SYMBOL_EXPORT libatgw_inner_v1_c_context __cdecl libatgw_inner_v1_c_create() { 
    atframe::gateway::libatgw_proto_inner_v1* ret = new (std::nothrow) atframe::gateway::libatgw_proto_inner_v1();
    if (NULL != ret) {
        ret->set_callbacks(libatgw_inner_v1_c_get_proto_callbacks());
    }
    return ret;
}

ATFRAME_SYMBOL_EXPORT void __cdecl libatgw_inner_v1_c_destroy(libatgw_inner_v1_c_context context) {
    delete ((::atframe::gateway::libatgw_proto_inner_v1 *)context);
}

ATFRAME_SYMBOL_EXPORT void __cdecl libatgw_inner_v1_c_set_recv_buffer_limit(libatgw_inner_v1_c_context context, uint64_t max_size, uint64_t max_number) {
    if (NULL == context) {
        return;
    }

    ((::atframe::gateway::libatgw_proto_inner_v1 *)context)->set_recv_buffer_limit((size_t)max_size, (size_t)max_number);
}

ATFRAME_SYMBOL_EXPORT void __cdecl libatgw_inner_v1_c_set_send_buffer_limit(libatgw_inner_v1_c_context context, uint64_t max_size, uint64_t max_number) {
    if (NULL == context) {
        return;
    }

    ((::atframe::gateway::libatgw_proto_inner_v1 *)context)->set_send_buffer_limit((size_t)max_size, (size_t)max_number);
}

ATFRAME_SYMBOL_EXPORT int32_t __cdecl libatgw_inner_v1_c_start_session(libatgw_inner_v1_c_context context) {
    if (NULL == context) {
        return ::atframe::gateway::error_code_t::EN_ECT_PARAM;
    }

    return ((::atframe::gateway::libatgw_proto_inner_v1 *)context)->start_session();
}

ATFRAME_SYMBOL_EXPORT int32_t __cdecl libatgw_inner_v1_c_reconnect_session(libatgw_inner_v1_c_context context, uint64_t sessios_id, int32_t crypt_type,
                                                                           const unsigned char *secret_buf, uint64_t secret_len, uint32_t keybits) {
    if (NULL == context) {
        return ::atframe::gateway::error_code_t::EN_ECT_PARAM;
    }

    std::vector<unsigned char> secret;
    secret.assign(secret_buf, secret_buf + secret_len);
    return ((::atframe::gateway::libatgw_proto_inner_v1 *)context)->reconnect_session(sessios_id, crypt_type, secret, keybits);
}

ATFRAME_SYMBOL_EXPORT void __cdecl libatgw_inner_v1_c_get_info(libatgw_inner_v1_c_context context, char *info_str, uint64_t info_len) {
    if (NULL == info_str || 0 == info_len) {
        return;
    }

    if (NULL == context) {
        info_str[0] = 0;
        return;
    }

    std::string msg = ((::atframe::gateway::libatgw_proto_inner_v1 *)context)->get_info();
    size_t len = msg.size();
    if (len >= info_len) {
        len = info_len - 1;
    }

    memcpy(info_str, msg.c_str(), len);
    info_str[len - 1] = 0;
}

ATFRAME_SYMBOL_EXPORT void __cdecl libatgw_inner_v1_c_set_private_data(libatgw_inner_v1_c_context context, void *p) {
    if (NULL == context) {
        return;
    }

    return ((::atframe::gateway::libatgw_proto_inner_v1 *)context)->set_private_data(p);
}

ATFRAME_SYMBOL_EXPORT void *__cdecl libatgw_inner_v1_c_get_private_data(libatgw_inner_v1_c_context context) {
    if (NULL == context) {
        return NULL;
    }

    return ((::atframe::gateway::libatgw_proto_inner_v1 *)context)->get_private_data();
}

ATFRAME_SYMBOL_EXPORT uint64_t __cdecl libatgw_inner_v1_c_get_session_id(libatgw_inner_v1_c_context context) {
    if (NULL == context) {
        return 0;
    }

    return ((::atframe::gateway::libatgw_proto_inner_v1 *)context)->get_session_id();
}

ATFRAME_SYMBOL_EXPORT int32_t __cdecl libatgw_inner_v1_c_get_crypt_type(libatgw_inner_v1_c_context context) {
    if (NULL == context) {
        return 0;
    }

    return ((::atframe::gateway::libatgw_proto_inner_v1 *)context)->get_crypt_handshake()->type;
}

ATFRAME_SYMBOL_EXPORT uint64_t __cdecl libatgw_inner_v1_c_get_crypt_secret_size(libatgw_inner_v1_c_context context) {
    if (NULL == context) {
        return 0;
    }

    return (uint64_t)(((::atframe::gateway::libatgw_proto_inner_v1 *)context)->get_crypt_handshake()->secret.size());
}

ATFRAME_SYMBOL_EXPORT uint64_t __cdecl libatgw_inner_v1_c_copy_crypt_secret(libatgw_inner_v1_c_context context, unsigned char *secret,
                                                                            uint64_t available_size) {
    if (NULL == context || 0 == available_size) {
        return 0;
    }

    size_t len = ((::atframe::gateway::libatgw_proto_inner_v1 *)context)->get_crypt_handshake()->secret.size();
    if (len >= available_size) {
        len = (size_t)available_size;
    }

    memcpy(secret, ((::atframe::gateway::libatgw_proto_inner_v1 *)context)->get_crypt_handshake()->secret.data(), len);
    return len;
}

ATFRAME_SYMBOL_EXPORT uint32_t __cdecl libatgw_inner_v1_c_get_crypt_keybits(libatgw_inner_v1_c_context context) {
    if (NULL == context) {
        return 0;
    }

    return ((::atframe::gateway::libatgw_proto_inner_v1 *)context)->get_crypt_handshake()->keybits;
}

ATFRAME_SYMBOL_EXPORT void __cdecl libatgw_inner_v1_c_read_alloc(libatgw_inner_v1_c_context context, uint64_t suggested_size, char **out_buf,
                                                                 uint64_t *out_len) {
    if (NULL == context) {
        return;
    }

    char *co = NULL;
    size_t colen = 0;
    if (NULL == out_buf) {
        out_buf = &co;
    }

    ((::atframe::gateway::libatgw_proto_inner_v1 *)context)->alloc_recv_buffer(suggested_size, *out_buf, colen);

    if (NULL != out_len) {
        *out_len = colen;
    }
}

ATFRAME_SYMBOL_EXPORT void __cdecl libatgw_inner_v1_c_read(libatgw_inner_v1_c_context context, int32_t ssz, const char *buff, uint64_t len, int32_t *errcode) {
    if (NULL == context) {
        return;
    }

    int32_t ecd = 0;

    ((::atframe::gateway::libatgw_proto_inner_v1 *)context)->read(ssz, buff, (size_t)len, ecd);

    if (NULL != errcode) {
        *errcode = ecd;
    }
}

ATFRAME_SYMBOL_EXPORT int32_t __cdecl libatgw_inner_v1_c_write_done(libatgw_inner_v1_c_context context, int32_t status) {
    if (NULL == context) {
        return ::atframe::gateway::error_code_t::EN_ECT_PARAM;
    }

    return ((::atframe::gateway::libatgw_proto_inner_v1 *)context)->write_done(status);
}

ATFRAME_SYMBOL_EXPORT int32_t __cdecl libatgw_inner_v1_c_post_msg(libatgw_inner_v1_c_context context, const void *out_buf, uint64_t out_len) {
    if (NULL == context) {
        return ::atframe::gateway::error_code_t::EN_ECT_PARAM;
    }

    return ((::atframe::gateway::libatgw_proto_inner_v1 *)context)->send_post(out_buf, out_len);
}

ATFRAME_SYMBOL_EXPORT int32_t __cdecl libatgw_inner_v1_c_send_ping(libatgw_inner_v1_c_context context) {
    if (NULL == context) {
        return ::atframe::gateway::error_code_t::EN_ECT_PARAM;
    }

    return ((::atframe::gateway::libatgw_proto_inner_v1 *)context)->send_ping();
}

ATFRAME_SYMBOL_EXPORT int64_t __cdecl libatgw_inner_v1_c_get_ping_delta(libatgw_inner_v1_c_context context) {
    if (NULL == context) {
        return 0;
    }

    return (int64_t)(((::atframe::gateway::libatgw_proto_inner_v1 *)context)->get_last_ping().last_delta);
}

ATFRAME_SYMBOL_EXPORT int32_t __cdecl libatgw_inner_v1_c_close(libatgw_inner_v1_c_context context, int32_t reason) {
    if (NULL == context) {
        return ::atframe::gateway::error_code_t::EN_ECT_PARAM;
    }

    return ((::atframe::gateway::libatgw_proto_inner_v1 *)context)->close(reason);
}

ATFRAME_SYMBOL_EXPORT int32_t __cdecl libatgw_inner_v1_c_is_closing(libatgw_inner_v1_c_context context) {
    if (NULL == context) {
        return 0;
    }

    return ((::atframe::gateway::libatgw_proto_inner_v1 *)context)->check_flag(::atframe::gateway::proto_base::flag_t::EN_PFT_CLOSING);
}

ATFRAME_SYMBOL_EXPORT int32_t __cdecl libatgw_inner_v1_c_is_closed(libatgw_inner_v1_c_context context) {
    if (NULL == context) {
        return 0;
    }

    return ((::atframe::gateway::libatgw_proto_inner_v1 *)context)->check_flag(::atframe::gateway::proto_base::flag_t::EN_PFT_CLOSED);
}

ATFRAME_SYMBOL_EXPORT int32_t __cdecl libatgw_inner_v1_c_is_handshake_updating(libatgw_inner_v1_c_context context) {
    if (NULL == context) {
        return 0;
    }

    return ((::atframe::gateway::libatgw_proto_inner_v1 *)context)->check_flag(::atframe::gateway::proto_base::flag_t::EN_PFT_HANDSHAKE_UPDATE);
}

ATFRAME_SYMBOL_EXPORT int32_t __cdecl libatgw_inner_v1_c_is_handshake_done(libatgw_inner_v1_c_context context) {
    if (NULL == context) {
        return 0;
    }

    return ((::atframe::gateway::libatgw_proto_inner_v1 *)context)->check_flag(::atframe::gateway::proto_base::flag_t::EN_PFT_HANDSHAKE_DONE);
}

ATFRAME_SYMBOL_EXPORT int32_t __cdecl libatgw_inner_v1_c_is_writing(libatgw_inner_v1_c_context context) {
    if (NULL == context) {
        return 0;
    }

    return ((::atframe::gateway::libatgw_proto_inner_v1 *)context)->check_flag(::atframe::gateway::proto_base::flag_t::EN_PFT_WRITING);
}

ATFRAME_SYMBOL_EXPORT int32_t __cdecl libatgw_inner_v1_c_is_in_callback(libatgw_inner_v1_c_context context) {
    if (NULL == context) {
        return 0;
    }

    return ((::atframe::gateway::libatgw_proto_inner_v1 *)context)->check_flag(::atframe::gateway::proto_base::flag_t::EN_PFT_IN_CALLBACK);
}

#ifdef __cplusplus
}
#endif