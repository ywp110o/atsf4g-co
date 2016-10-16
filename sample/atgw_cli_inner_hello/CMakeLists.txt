# ============ sample - [...] ============
get_filename_component(SAMPLE_SRC_BIN_NAME ${CMAKE_CURRENT_LIST_DIR} NAME_WE)
set(SAMPLE_SRC_BIN_NAME "sample_${SAMPLE_SRC_BIN_NAME}")
EchoWithColor(COLOR GREEN "-- Configure ${SAMPLE_SRC_BIN_NAME} on ${CMAKE_CURRENT_LIST_DIR}")

aux_source_directory(${CMAKE_CURRENT_LIST_DIR} SAMPLE_SRC_LIST)
set(EXECUTABLE_OUTPUT_PATH "${PROJECT_BINARY_DIR}/sample")

# special setting for sample
add_compiler_define(ATBUS_MACRO_MSG_LIMIT=65536)
include_directories("${ATFRAMEWORK_BASE_DIR}/service/atgateway/protocols" 
    ${ATFRAMEWORK_SERVICE_COMPONENT_DIR} 
    "${ATFRAMEWORK_BASE_DIR}/export/atgw_inner_v1_c")

# ===== For inner protocol, some macro may has effect on code:
# ATFRAME_GATEWAY_MACRO_DATA_SMALL_SIZE: used for merge small package(default: 3072)
# ATBUS_MACRO_MSG_LIMIT: max message length, can not be greater than it in atgateway(default: 65536,64K)

# ===== We should also install the files listed below:
# ${ATFRAMEWORK_ATFRAME_UTILS_INC_DIR}/std/thread.h
# ${ATFRAMEWORK_ATFRAME_UTILS_INC_DIR}/std/smart_ptr.h
# ${ATFRAMEWORK_ATFRAME_UTILS_INC_DIR}/std/functional.h
# ${ATFRAMEWORK_ATFRAME_UTILS_INC_DIR}/common/string_oprs.h
# ${ATFRAMEWORK_ATFRAME_UTILS_INC_DIR}/design_pattern/noncopyable.h
# ${ATFRAMEWORK_ATFRAME_UTILS_INC_DIR}/lock/atomic_int_type.h
# ${ATFRAMEWORK_ATFRAME_UTILS_INC_DIR}/lock/lock_holder.h
# ${ATFRAMEWORK_ATFRAME_UTILS_INC_DIR}/lock/spin_lock.h
# ${ATFRAMEWORK_ATFRAME_UTILS_INC_DIR}/lock/seq_alloc.h
# ${ATFRAMEWORK_ATFRAME_UTILS_INC_DIR}/algorithm/xxtea.h
# ${ATFRAMEWORK_ATFRAME_UTILS_INC_DIR}/algorithm/murmur_hash.h
# ${ATFRAMEWORK_ATBUS_INC_DIR}/detail/buffer.h
# ${ATFRAMEWORK_BASE_DIR}/service/atgateway/protocols/proto_base.h

# ${ATFRAMEWORK_BASE_DIR}/service/atgateway/protocols/proto_base.cpp
# ${ATFRAMEWORK_BASE_DIR}/service/atgateway/protocols/inner_v1/*
# ${ATFRAMEWORK_ATFRAME_UTILS_SRC_DIR}/algorithm/xxtea.cpp
# ${ATFRAMEWORK_ATFRAME_UTILS_SRC_DIR}/algorithm/murmur_hash.cpp
# ${ATFRAMEWORK_ATBUS_SRC_DIR}/detail/buffer.cpp

# crypt library check
if (OPENSSL_FOUND)
    add_compiler_define(LIBATFRAME_ATGATEWAY_ENABLE_OPENSSL=1)
elseif(LIBRESSL_FOUND)
    add_compiler_define(LIBATFRAME_ATGATEWAY_ENABLE_LIBRESSL=1)
elseif(MBEDTLS_FOUND)
    add_compiler_define(LIBATFRAME_ATGATEWAY_ENABLE_MBEDTLS=1)
endif()

if (MINGW)
    list(APPEND EXTENTION_LINK_LIB stdc++)
endif()

add_executable(${SAMPLE_SRC_BIN_NAME} ${SAMPLE_SRC_LIST})
target_link_libraries(${SAMPLE_SRC_BIN_NAME}
    libatgw_inner_v1_c
    ${3RD_PARTY_LIBUV_LINK_NAME}
    ${3RD_PARTY_CRYPT_LINK_NAME}
    ${EXTENTION_LINK_LIB}
)