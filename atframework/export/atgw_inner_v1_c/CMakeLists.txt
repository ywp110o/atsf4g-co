# ============ export - [...] ============
get_filename_component(EXPORT_SRC_BIN_NAME ${CMAKE_CURRENT_LIST_DIR} NAME_WE)
if(WIN32 AND NOT MINGW AND NOT CYGWIN)
    set(EXPORT_SRC_BIN_NAME ${EXPORT_LIBATGW_INNER_V1_C_BIN_NAME})
else()
    set(EXPORT_SRC_BIN_NAME ${EXPORT_LIBATGW_INNER_V1_C_BIN_NAME})
endif()

EchoWithColor(COLOR GREEN "-- Configure ${EXPORT_SRC_BIN_NAME} on ${CMAKE_CURRENT_LIST_DIR}")

aux_source_directory(${CMAKE_CURRENT_LIST_DIR} EXPORT_SRC_LIST)

# special setting for libatgw_inner_v1_c
add_compiler_define(ATBUS_MACRO_MSG_LIMIT=${ATBUS_MACRO_MSG_LIMIT})
include_directories("${ATFRAMEWORK_BASE_DIR}/service/atgateway/protocols" ${ATFRAMEWORK_SERVICE_COMPONENT_DIR})

list(APPEND EXPORT_SRC_LIST ${ATFRAMEWORK_BASE_DIR}/service/atgateway/protocols/proto_base.cpp)
list(APPEND EXPORT_SRC_LIST ${ATFRAMEWORK_BASE_DIR}/service/atgateway/protocols/inner_v1/libatgw_proto_inner.cpp)
list(APPEND EXPORT_SRC_LIST ${ATFRAMEWORK_ATFRAME_UTILS_SRC_DIR}/algorithm/xxtea.cpp)
list(APPEND EXPORT_SRC_LIST ${ATFRAMEWORK_ATFRAME_UTILS_SRC_DIR}/algorithm/crypto_cipher.cpp)
list(APPEND EXPORT_SRC_LIST ${ATFRAMEWORK_ATFRAME_UTILS_SRC_DIR}/algorithm/murmur_hash.cpp)
list(APPEND EXPORT_SRC_LIST ${ATFRAMEWORK_ATBUS_SRC_DIR}/detail/buffer.cpp)

set (EXPORT_SOURCE_DIR "${PROJECT_BINARY_DIR}/export/${EXPORT_SRC_BIN_NAME}")
set (EXPORT_SOURCE_INC_DIR "${EXPORT_SOURCE_DIR}/include")
set (EXPORT_SOURCE_INC_CFG_DIR "${EXPORT_SOURCE_INC_DIR}/config")
set (EXPORT_SOURCE_SRC_DIR "${EXPORT_SOURCE_DIR}/src")
set (EXPORT_SOURCE_CMAKE_DIR "${EXPORT_SOURCE_DIR}/cmake_modules")
file(MAKE_DIRECTORY ${EXPORT_SOURCE_INC_DIR})
file(MAKE_DIRECTORY ${EXPORT_SOURCE_INC_CFG_DIR})
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

file(MAKE_DIRECTORY "${EXPORT_SOURCE_SRC_DIR}/config")
file(COPY 
    "${ATFRAMEWORK_ATFRAME_UTILS_INC_DIR}/config/compile_optimize.h"
    DESTINATION ${EXPORT_SOURCE_INC_CFG_DIR}
    USE_SOURCE_PERMISSIONS
)
file(COPY 
    "${ATFRAMEWORK_ATFRAME_UTILS_INC_DIR}/config/compiler_features.h"
    "${ATFRAMEWORK_ATFRAME_UTILS_INC_DIR}/config/atframe_utils_build_feature.h.in"
    DESTINATION "${EXPORT_SOURCE_SRC_DIR}/config"
    USE_SOURCE_PERMISSIONS
)

file(MAKE_DIRECTORY "${EXPORT_SOURCE_SRC_DIR}/common")
file(COPY 
    "${ATFRAMEWORK_ATFRAME_UTILS_INC_DIR}/common/string_oprs.h"
    "${ATFRAMEWORK_ATFRAME_UTILS_INC_DIR}/common/compiler_message.h"
    "${ATFRAMEWORK_ATFRAME_UTILS_INC_DIR}/common/file_system.h"
    "${ATFRAMEWORK_ATFRAME_UTILS_SRC_DIR}/common/file_system.cpp"
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

# ----- copy script files -----
file(COPY "${CMAKE_CURRENT_LIST_DIR}/build_ios.sh" DESTINATION ${EXPORT_SOURCE_DIR} USE_SOURCE_PERMISSIONS)
file(COPY "${CMAKE_CURRENT_LIST_DIR}/build_android.sh" DESTINATION ${EXPORT_SOURCE_DIR} USE_SOURCE_PERMISSIONS)

add_library(${EXPORT_SRC_BIN_NAME} SHARED ${EXPORT_SRC_LIST})
target_link_libraries(${EXPORT_SRC_BIN_NAME}
    ${3RD_PARTY_CRYPT_LINK_NAME}
    ${COMPILER_OPTION_EXTERN_CXX_LIBS}
)
