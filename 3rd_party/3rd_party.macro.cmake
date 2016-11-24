# =========== 3rd_party ===========
unset (PROJECT_3RD_PARTY_SRC_LIST)

# =========== 3rd_party - msgpack ===========
include("${PROJECT_3RD_PARTY_ROOT_DIR}/msgpack/msgpack.cmake")

# =========== 3rd_party - libuv ===========
include("${PROJECT_3RD_PARTY_ROOT_DIR}/libuv/libuv.cmake")

# =========== 3rd_party - libiniloader ===========
include("${PROJECT_3RD_PARTY_ROOT_DIR}/libiniloader/libiniloader.cmake")

# =========== 3rd_party - libcurl ===========
include("${PROJECT_3RD_PARTY_ROOT_DIR}/libcurl/libcurl.cmake")

# =========== 3rd_party - rapidjson ===========
include("${PROJECT_3RD_PARTY_ROOT_DIR}/rapidjson/rapidjson.cmake")

# =========== 3rd_party - flatbuffers ===========
include("${PROJECT_3RD_PARTY_ROOT_DIR}/flatbuffers/flatbuffers.cmake")

# =========== 3rd_party - jemalloc ===========
if(NOT MSVC OR PROJECT_ENABLE_JEMALLOC)
    include("${PROJECT_3RD_PARTY_ROOT_DIR}/jemalloc/jemalloc.cmake")
endif()

# =========== 3rd_party - openssl/libressl/mbedtls ===========
include("${PROJECT_3RD_PARTY_ROOT_DIR}/openssl/openssl.cmake")
if (NOT OPENSSL_FOUND)
    include("${PROJECT_3RD_PARTY_ROOT_DIR}/libressl/libressl.cmake")
    if (NOT LIBRESSL_FOUND)
        include("${PROJECT_3RD_PARTY_ROOT_DIR}/mbedtls/mbedtls.cmake")

        if (NOT MBEDTLS_FOUND)
            message(FATAL_ERROR "must at least have one of openssl,libressl or mbedtls.")
        else()
            list(APPEND 3RD_PARTY_CRYPT_LINK_NAME ${MbedTLS_CRYPTO_LIBRARIES})
        endif()
    else()
        list(APPEND 3RD_PARTY_CRYPT_LINK_NAME ${LIBRESSL_CRYPTO_LIBRARY})
    endif()
else()
    list(APPEND 3RD_PARTY_CRYPT_LINK_NAME ${OPENSSL_CRYPTO_LIBRARY})
endif()

if (MINGW)
    EchoWithColor(COLOR GREEN "-- MinGW: custom add lib gdi32")
    list(APPEND 3RD_PARTY_CRYPT_LINK_NAME gdi32)
endif()

# =========== 3rd_party - libcopp ===========
include("${PROJECT_3RD_PARTY_ROOT_DIR}/libcopp/libcopp.cmake")
## 导入所有工程项目
add_project_recurse(${CMAKE_CURRENT_LIST_DIR})