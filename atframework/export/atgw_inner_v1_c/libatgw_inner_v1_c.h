#ifndef _ATFRAME_EXPORT_ATGATEWAY_INNER_PROTO_V1_H_
#define _ATFRAME_EXPORT_ATGATEWAY_INNER_PROTO_V1_H_

#pragma once

#include <stddef.h>
#include <stdint.h>


#if !defined(ATFRAME_SYMBOL_EXPORT) && defined(_MSC_VER)
#define ATFRAME_SYMBOL_EXPORT __declspec(dllexport)

#elif !defined(ATFRAME_SYMBOL_EXPORT) && defined(__GNUC__)

#ifndef __cdecl
// see https://gcc.gnu.org/onlinedocs/gcc-4.0.0/gcc/Function-Attributes.html
// Intel x86 architecture specific calling conventions
#ifdef _M_IX86
#define __cdecl __attribute__((__cdecl__))
#else
#define __cdecl
#endif
#endif

#if defined(__clang__)

#if !defined(_WIN32) && !defined(__WIN32__) && !defined(WIN32)
#define ATFRAME_SYMBOL_EXPORT __attribute__((__visibility__("default")))
#endif

#else

#if __GNUC__ >= 4
#if (defined(_WIN32) || defined(__WIN32__) || defined(WIN32)) && !defined(__CYGWIN__)
#define ATFRAME_SYMBOL_EXPORT __attribute__((__dllexport__))
#else
#define ATFRAME_SYMBOL_EXPORT __attribute__((__visibility__("default")))
#endif
#endif

#endif

#endif

#ifndef ATFRAME_SYMBOL_EXPORT
#define ATFRAME_SYMBOL_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef union {
    void *pa;
    uintptr_t pu;
    intptr_t pi;
} libatgw_inner_v1_c_context;

typedef int32_t (*libatgw_inner_v1_c_on_write_start_fn_t)(libatgw_inner_v1_c_context, void *, uint64_t, int32_t *);
typedef int32_t (*libatgw_inner_v1_c_on_message_fn_t)(libatgw_inner_v1_c_context, const void *, uint64_t);
typedef int32_t (*libatgw_inner_v1_c_on_init_new_session_fn_t)(libatgw_inner_v1_c_context, uint64_t *);
typedef int32_t (*libatgw_inner_v1_c_on_init_reconnect_fn_t)(libatgw_inner_v1_c_context, uint64_t);
typedef int32_t (*libatgw_inner_v1_c_on_close_fn_t)(libatgw_inner_v1_c_context, int32_t);
typedef int32_t (*libatgw_inner_v1_c_on_handshake_done_fn_t)(libatgw_inner_v1_c_context, int32_t);
typedef int32_t (*libatgw_inner_v1_c_on_error_fn_t)(libatgw_inner_v1_c_context, const char *, int32_t, int32_t, const char *);


ATFRAME_SYMBOL_EXPORT void __cdecl libatgw_inner_v1_c_gset_on_write_start_fn(libatgw_inner_v1_c_on_write_start_fn_t fn);
ATFRAME_SYMBOL_EXPORT void __cdecl libatgw_inner_v1_c_gset_on_message_fn(libatgw_inner_v1_c_on_message_fn_t fn);
ATFRAME_SYMBOL_EXPORT void __cdecl libatgw_inner_v1_c_gset_on_init_new_session_fn(libatgw_inner_v1_c_on_init_new_session_fn_t fn);
ATFRAME_SYMBOL_EXPORT void __cdecl libatgw_inner_v1_c_gset_on_init_reconnect_fn(libatgw_inner_v1_c_on_init_reconnect_fn_t fn);
ATFRAME_SYMBOL_EXPORT void __cdecl libatgw_inner_v1_c_gset_on_close_fn(libatgw_inner_v1_c_on_close_fn_t fn);
ATFRAME_SYMBOL_EXPORT void __cdecl libatgw_inner_v1_c_gset_on_handshake_done_fn(libatgw_inner_v1_c_on_handshake_done_fn_t fn);
ATFRAME_SYMBOL_EXPORT void __cdecl libatgw_inner_v1_c_gset_on_handshake_update_fn(libatgw_inner_v1_c_on_handshake_done_fn_t fn);
ATFRAME_SYMBOL_EXPORT void __cdecl libatgw_inner_v1_c_gset_on_error_fn(libatgw_inner_v1_c_on_error_fn_t fn);

ATFRAME_SYMBOL_EXPORT libatgw_inner_v1_c_context __cdecl libatgw_inner_v1_c_create();
ATFRAME_SYMBOL_EXPORT void __cdecl libatgw_inner_v1_c_destroy(libatgw_inner_v1_c_context context);

ATFRAME_SYMBOL_EXPORT void __cdecl libatgw_inner_v1_c_set_recv_buffer_limit(libatgw_inner_v1_c_context context, uint64_t max_size, uint64_t max_number);
ATFRAME_SYMBOL_EXPORT void __cdecl libatgw_inner_v1_c_set_send_buffer_limit(libatgw_inner_v1_c_context context, uint64_t max_size, uint64_t max_number);

ATFRAME_SYMBOL_EXPORT int32_t __cdecl libatgw_inner_v1_c_start_session(libatgw_inner_v1_c_context context);
ATFRAME_SYMBOL_EXPORT int32_t __cdecl libatgw_inner_v1_c_reconnect_session(libatgw_inner_v1_c_context context, uint64_t sessios_id, int32_t crypt_type,
                                                                           const unsigned char *secret_buf, uint64_t secret_len, uint32_t keybits);

ATFRAME_SYMBOL_EXPORT void __cdecl libatgw_inner_v1_c_get_info(libatgw_inner_v1_c_context context, char *info_str, uint64_t info_len);

ATFRAME_SYMBOL_EXPORT void __cdecl libatgw_inner_v1_c_set_private_data(libatgw_inner_v1_c_context context, void *);
ATFRAME_SYMBOL_EXPORT void *__cdecl libatgw_inner_v1_c_get_private_data(libatgw_inner_v1_c_context context);

ATFRAME_SYMBOL_EXPORT uint64_t __cdecl libatgw_inner_v1_c_get_session_id(libatgw_inner_v1_c_context context);
ATFRAME_SYMBOL_EXPORT int32_t __cdecl libatgw_inner_v1_c_get_crypt_type(libatgw_inner_v1_c_context context);
ATFRAME_SYMBOL_EXPORT uint64_t __cdecl libatgw_inner_v1_c_get_crypt_secret_size(libatgw_inner_v1_c_context context);
ATFRAME_SYMBOL_EXPORT uint64_t __cdecl libatgw_inner_v1_c_copy_crypt_secret(libatgw_inner_v1_c_context context, unsigned char *secret, uint64_t available_size);
ATFRAME_SYMBOL_EXPORT uint32_t __cdecl libatgw_inner_v1_c_get_crypt_keybits(libatgw_inner_v1_c_context context);

ATFRAME_SYMBOL_EXPORT void __cdecl libatgw_inner_v1_c_read_alloc(libatgw_inner_v1_c_context context, uint64_t suggested_size, char **out_buf,
                                                                 uint64_t *out_len);

/**
 * @brief mark how much data already copied into read buffer.
 * @param ssz context protocol context
 * @param ssz nread, error code or data length. useless
 * @param buff start address of read data. useless
 * @param len lengtn of read data. read buffer manager will cost len bytes and try to dispatch message
 * @param errcode where to receive error code
 */
ATFRAME_SYMBOL_EXPORT void __cdecl libatgw_inner_v1_c_read(libatgw_inner_v1_c_context context, int32_t ssz, const char *buff, uint64_t len, int32_t *errcode);

ATFRAME_SYMBOL_EXPORT int32_t __cdecl libatgw_inner_v1_c_write_done(libatgw_inner_v1_c_context context, int32_t status);

ATFRAME_SYMBOL_EXPORT int32_t __cdecl libatgw_inner_v1_c_post_msg(libatgw_inner_v1_c_context context, const void *out_buf, uint64_t out_len);
ATFRAME_SYMBOL_EXPORT int32_t __cdecl libatgw_inner_v1_c_send_ping(libatgw_inner_v1_c_context context);
ATFRAME_SYMBOL_EXPORT int64_t __cdecl libatgw_inner_v1_c_get_ping_delta(libatgw_inner_v1_c_context context);

ATFRAME_SYMBOL_EXPORT int32_t __cdecl libatgw_inner_v1_c_close(libatgw_inner_v1_c_context context, int32_t reason);

ATFRAME_SYMBOL_EXPORT int32_t __cdecl libatgw_inner_v1_c_is_closing(libatgw_inner_v1_c_context context);
ATFRAME_SYMBOL_EXPORT int32_t __cdecl libatgw_inner_v1_c_is_closed(libatgw_inner_v1_c_context context);
ATFRAME_SYMBOL_EXPORT int32_t __cdecl libatgw_inner_v1_c_is_handshake_updating(libatgw_inner_v1_c_context context);
ATFRAME_SYMBOL_EXPORT int32_t __cdecl libatgw_inner_v1_c_is_handshake_done(libatgw_inner_v1_c_context context);
ATFRAME_SYMBOL_EXPORT int32_t __cdecl libatgw_inner_v1_c_is_writing(libatgw_inner_v1_c_context context);
ATFRAME_SYMBOL_EXPORT int32_t __cdecl libatgw_inner_v1_c_is_in_callback(libatgw_inner_v1_c_context context);

#ifdef __cplusplus
}
#endif

#endif
