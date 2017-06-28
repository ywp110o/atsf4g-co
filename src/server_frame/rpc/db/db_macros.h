//
// Created by owt50 on 2016/9/28.
//

#ifndef _RPC_DB_DB_MACROS_H
#define _RPC_DB_DB_MACROS_H

#pragma once

#include <cstddef>
#include <stdint.h>
#include <inttypes.h>

#include <common/string_oprs.h>

#define DBGETGLOBALKEY_FMT(table_name) "g:" #table_name ":%s"

#define DBGETGLOBALKEY_ARG(id) id

#define DBGETTABLEKEY_FMT(table_name) #table_name ":%s"

// get zone id
#define DBGETTABLEKEY_ARG(openid) openid

#define DBGETZONETABLEKEY_ARG(zone, openid) zone, openid

#define DBUSERCMD(table_name, cmd, openid, format, args...) #cmd " " DBGETTABLEKEY_FMT(table_name) " " format, DBGETTABLEKEY_ARG(openid), ##args

#define DBZONEUSERCMD(zone, table_name, cmd, openid, format, args...) \
    #cmd " " DBGETTABLEKEY_FMT(table_name) " " format, DBGETZONETABLEKEY_ARG(zone, openid), ##args

#define DBUSERKEYWITHVAR(table_name, keyvar, keylen, v)                                                                                              \
    {                                                                                                                                                \
        int __snprintf_writen_length = UTIL_STRFUNC_SNPRINTF(keyvar, static_cast<int>(keylen), DBGETTABLEKEY_FMT(table_name), DBGETTABLEKEY_ARG(v)); \
        if (__snprintf_writen_length < 0) {                                                                                                          \
            keyvar[sizeof(keyvar) - 1] = '\0';                                                                                                       \
            keylen = 0;                                                                                                                              \
        } else {                                                                                                                                     \
            keylen = static_cast<size_t>(__snprintf_writen_length);                                                                                  \
            keyvar[__snprintf_writen_length] = '\0';                                                                                                 \
        }                                                                                                                                            \
    }

#define DBZONEUSERKEYWITHVAR(zone, table_name, keyvar, keylen, v)                                                                                              \
    {                                                                                                                                                          \
        int __snprintf_writen_length = UTIL_STRFUNC_SNPRINTF(keyvar, static_cast<int>(keylen), DBGETTABLEKEY_FMT(table_name), DBGETZONETABLEKEY_ARG(zone, v)); \
        if (__snprintf_writen_length < 0) {                                                                                                                    \
            keyvar[sizeof(keyvar) - 1] = '\0';                                                                                                                 \
            keylen = 0;                                                                                                                                        \
        } else {                                                                                                                                               \
            keylen = static_cast<size_t>(__snprintf_writen_length);                                                                                            \
            keyvar[__snprintf_writen_length] = '\0';                                                                                                           \
        }                                                                                                                                                      \
    }

#define DBUSERKEY(table_name, keyvar, keylen, v) \
    char keyvar[256];                            \
    size_t keylen = sizeof(keyvar) - 1;          \
    DBUSERKEYWITHVAR(table_name, keyvar, keylen, v)

#define DBZONEUSERKEY(zone, table_name, keyvar, keylen, v) \
    char keyvar[256];                                      \
    size_t keylen = sizeof(keyvar) - 1;                    \
    DBZONEUSERKEYWITHVAR(zone, table_name, keyvar, keylen, v)

#define DBGLOBALCMD(table_name, cmd, id, format, args...) #cmd " " DBGETGLOBALKEY_FMT(table_name) " " format, DBGETGLOBALKEY_ARG(id), ##args

#define DBGLOBALKEY(table_name, keyvar, keylen, v)                                                                                      \
    char keyvar[256];                                                                                                                   \
    size_t keylen = 0;                                                                                                                  \
    {                                                                                                                                   \
        int __snprintf_writen_length =                                                                                                  \
            UTIL_STRFUNC_SNPRINTF(keyvar, static_cast<int>(sizeof(keyvar)) - 1, DBGETGLOBALKEY_FMT(table_name), DBGETGLOBALKEY_ARG(v)); \
        if (__snprintf_writen_length < 0) {                                                                                             \
            keyvar[sizeof(keyvar) - 1] = keyvar[0] = '\0';                                                                              \
        } else {                                                                                                                        \
            keylen = static_cast<size_t>(__snprintf_writen_length);                                                                     \
            keyvar[__snprintf_writen_length] = '\0';                                                                                    \
        }                                                                                                                               \
    }


// ################# macros for tables #################
#define DBLOGINCMD(cmd, openid, format, args...) DBUSERCMD(login, cmd, openid, format, ##args)
#define DBLOGINKEY(keyvar, keylen, v) DBUSERKEY(login, keyvar, keylen, v)

#define DBZONELOGINCMD(zone, cmd, openid, format, args...) DBZONEUSERCMD(zone, login, cmd, openid, format, ##args)
#define DBZONELOGINKEY(zone, keyvar, keylen, v) DBZONEUSERKEY(zone, login, keyvar, keylen, v)

#define DBPLAYERCMD(cmd, openid, format, args...) DBUSERCMD(player, cmd, openid, format, ##args)
#define DBPLAYERKEY(keyvar, keylen, v) DBUSERKEY(player, keyvar, keylen, v)

#endif //_RPC_DB_DB_MACROS_H
