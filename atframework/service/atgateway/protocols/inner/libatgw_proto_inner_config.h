#ifndef _ATFRAME_SERVICE_ATGATEWAY_PROTO_INNER_CONFIG_H_
#define _ATFRAME_SERVICE_ATGATEWAY_PROTO_INNER_CONFIG_H_

#pragma once


#if defined(LIBATFRAME_ATGATEWAY_ENABLE_OPENSSL)
#elif defined(LIBATFRAME_ATGATEWAY_ENABLE_MBEDTLS)
#elif defined(LIBATFRAME_ATGATEWAY_ENABLE_LIBRESSL)
#endif

namespace atframe {
    namespace gateway {
        namespace proto {
            namespace inner {
                struct encrypt_type_t {
                    enum type {
                        EN_ET_NONE = 0, // no encrypt
                        EN_ET_XTEA,     // xtea
                        EN_ET_AES,      // aes
                    };
                };

                struct rsa_sign_t {
                    enum type {
                        EN_RST_PKCS1 = 0, // PKCS#1
                        EN_RST_PKCS1_V15, // PKCS#1 v1.5
                        EN_RST_PSS,       // PKCS#1 v2.1
                    };
                };

                struct hash_id_t {
                    enum type {
                        EN_HIT_MD5 = 0, // hash id: md5
                        EN_HIT_SHA1,    // hash id: sha1
                        EN_HIT_SHA256,  // hash id: sha256
                        EN_HIT_SHA512,  // hash id: sha512
                    };
                };
            }
        }
    }
}

#endif