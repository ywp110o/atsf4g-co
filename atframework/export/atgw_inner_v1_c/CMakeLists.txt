# ============ export - [...] ============
get_filename_component(EXPORT_SRC_BIN_NAME ${CMAKE_CURRENT_LIST_DIR} NAME_WE)
set(EXPORT_SRC_BIN_NAME "lib${EXPORT_SRC_BIN_NAME}")
EchoWithColor(COLOR GREEN "-- Configure ${EXPORT_SRC_BIN_NAME} on ${CMAKE_CURRENT_LIST_DIR}")

aux_source_directory(${CMAKE_CURRENT_LIST_DIR} EXPORT_SRC_LIST)
set(LIBRARY_OUTPUT_PATH "${PROJECT_BINARY_DIR}/export/lib")

# special setting for libatgw_inner_v1_c
add_compiler_define(ATBUS_MACRO_MSG_LIMIT=${ATBUS_MACRO_MSG_LIMIT})
include_directories("${ATFRAMEWORK_BASE_DIR}/service/atgateway/protocols" ${ATFRAMEWORK_SERVICE_COMPONENT_DIR})

list(APPEND EXPORT_SRC_LIST ${ATFRAMEWORK_BASE_DIR}/service/atgateway/protocols/proto_base.cpp)
list(APPEND EXPORT_SRC_LIST ${ATFRAMEWORK_BASE_DIR}/service/atgateway/protocols/inner_v1/libatgw_proto_inner.cpp)
list(APPEND EXPORT_SRC_LIST ${ATFRAMEWORK_ATFRAME_UTILS_SRC_DIR}/algorithm/xxtea.cpp)
list(APPEND EXPORT_SRC_LIST ${ATFRAMEWORK_ATFRAME_UTILS_SRC_DIR}/algorithm/murmur_hash.cpp)
list(APPEND EXPORT_SRC_LIST ${ATFRAMEWORK_ATBUS_SRC_DIR}/detail/buffer.cpp)

set (EXPORT_SOURCE_DIR "${PROJECT_BINARY_DIR}/export/${EXPORT_SRC_BIN_NAME}")
set (EXPORT_SOURCE_INC_DIR "${EXPORT_SOURCE_DIR}/include")
set (EXPORT_SOURCE_SRC_DIR "${EXPORT_SOURCE_DIR}/src")
set (EXPORT_SOURCE_CMAKE_DIR "${EXPORT_SOURCE_DIR}/cmake_modules")
file(MAKE_DIRECTORY ${EXPORT_SOURCE_INC_DIR})
file(MAKE_DIRECTORY ${EXPORT_SOURCE_SRC_DIR})
file(MAKE_DIRECTORY ${EXPORT_SOURCE_CMAKE_DIR})

# ===== For inner protocol, some macro may has effect on code:
# ATFRAME_GATEWAY_MACRO_DATA_SMALL_SIZE: used for merge small package(default: 3072)
# ATBUS_MACRO_MSG_LIMIT: max message length, can not be greater than it in atgateway(default: 65536,64K)

# ----- copy dependency utility files -----
file(COPY 
    "${3RD_PARTY_FLATBUFFER_INC_DIR}/flatbuffers"
    "${ATFRAMEWORK_ATFRAME_UTILS_INC_DIR}/std"
    "${ATFRAMEWORK_ATFRAME_UTILS_INC_DIR}/lock"
    "${ATFRAMEWORK_ATFRAME_UTILS_INC_DIR}/design_pattern"
    "${ATFRAMEWORK_ATFRAME_UTILS_INC_DIR}/algorithm"
    DESTINATION ${EXPORT_SOURCE_SRC_DIR}
    USE_SOURCE_PERMISSIONS
)

file(MAKE_DIRECTORY "${EXPORT_SOURCE_SRC_DIR}/common")
file(COPY 
    "${ATFRAMEWORK_ATFRAME_UTILS_INC_DIR}/common/string_oprs.h"
    "${ATFRAMEWORK_ATFRAME_UTILS_INC_DIR}/common/compiler_message.h"
    DESTINATION "${EXPORT_SOURCE_SRC_DIR}/common"
    USE_SOURCE_PERMISSIONS
)

file(COPY 
    "${ATFRAMEWORK_ATFRAME_UTILS_SRC_DIR}/algorithm"
    DESTINATION ${EXPORT_SOURCE_SRC_DIR}
    USE_SOURCE_PERMISSIONS
)

# ----- copy dependency atbus files -----
file(MAKE_DIRECTORY "${EXPORT_SOURCE_SRC_DIR}/detail")
file(COPY 
    "${ATFRAMEWORK_ATBUS_INC_DIR}/detail/buffer.h"
    "${ATFRAMEWORK_ATBUS_INC_DIR}/detail/libatbus_error.h"
    DESTINATION "${EXPORT_SOURCE_SRC_DIR}/detail"
    USE_SOURCE_PERMISSIONS
)

file(MAKE_DIRECTORY "${EXPORT_SOURCE_SRC_DIR}/detail")
file(COPY 
    "${ATFRAMEWORK_ATBUS_SRC_DIR}/detail/buffer.cpp"
    DESTINATION "${EXPORT_SOURCE_SRC_DIR}/detail"
    USE_SOURCE_PERMISSIONS
)

# ----- copy dependency atgateway protocol files -----
file(COPY 
    "${ATFRAMEWORK_BASE_DIR}/service/atgateway/protocols"
    DESTINATION ${EXPORT_SOURCE_SRC_DIR}
    USE_SOURCE_PERMISSIONS
    FILES_MATCHING PATTERN "*.h"
)

file(COPY 
    "${ATFRAMEWORK_BASE_DIR}/service/atgateway/protocols"
    DESTINATION ${EXPORT_SOURCE_SRC_DIR}
    USE_SOURCE_PERMISSIONS
    FILES_MATCHING PATTERN "*.cpp"
)

# ----- copy this project's files -----
file(COPY "${CMAKE_CURRENT_LIST_DIR}/libatgw_inner_v1_c.h"
    DESTINATION ${EXPORT_SOURCE_INC_DIR}
    USE_SOURCE_PERMISSIONS
)

file(COPY "${CMAKE_CURRENT_LIST_DIR}/libatgw_inner_v1_c.cpp"
    DESTINATION ${EXPORT_SOURCE_SRC_DIR}
    USE_SOURCE_PERMISSIONS
)

# ----- copy cmake files -----
file(COPY "${PROJECT_CMAKE_MODULE_DIR}/modules/EchoWithColor.cmake" DESTINATION ${EXPORT_SOURCE_CMAKE_DIR} USE_SOURCE_PERMISSIONS)
file(COPY "${PROJECT_CMAKE_MODULE_DIR}/modules/print_color.py" DESTINATION ${EXPORT_SOURCE_CMAKE_DIR} USE_SOURCE_PERMISSIONS)
file(COPY "${PROJECT_CMAKE_MODULE_DIR}/modules/FindConfigurePackage.cmake" DESTINATION ${EXPORT_SOURCE_CMAKE_DIR} USE_SOURCE_PERMISSIONS)
file(COPY "${PROJECT_CMAKE_MODULE_DIR}/CompilerOption.cmake" DESTINATION ${EXPORT_SOURCE_CMAKE_DIR} USE_SOURCE_PERMISSIONS)
file(COPY "${PROJECT_CMAKE_MODULE_DIR}/modules/FindMbedTLS.cmake" DESTINATION ${EXPORT_SOURCE_CMAKE_DIR} USE_SOURCE_PERMISSIONS)
file(COPY "${PROJECT_CMAKE_MODULE_DIR}/modules/FindLibreSSL.cmake" DESTINATION ${EXPORT_SOURCE_CMAKE_DIR} USE_SOURCE_PERMISSIONS)
file(COPY "${CMAKE_CURRENT_LIST_DIR}/CMakeLists.export.txt" DESTINATION ${EXPORT_SOURCE_DIR} USE_SOURCE_PERMISSIONS)
file(RENAME "${EXPORT_SOURCE_DIR}/CMakeLists.export.txt" "${EXPORT_SOURCE_DIR}/CMakeLists.txt")

# crypt library check
if (OPENSSL_FOUND)
    add_compiler_define(LIBATFRAME_ATGATEWAY_ENABLE_OPENSSL=1)
elseif(LIBRESSL_FOUND)
    add_compiler_define(LIBATFRAME_ATGATEWAY_ENABLE_LIBRESSL=1)
elseif(MBEDTLS_FOUND)
    add_compiler_define(LIBATFRAME_ATGATEWAY_ENABLE_MBEDTLS=1)
endif()

add_library(${EXPORT_SRC_BIN_NAME} SHARED ${EXPORT_SRC_LIST})
target_link_libraries(${EXPORT_SRC_BIN_NAME}
    ${3RD_PARTY_CRYPT_LINK_NAME}
    ${COMPILER_OPTION_EXTERN_CXX_LIBS}
)