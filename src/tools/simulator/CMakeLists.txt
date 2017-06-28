# ============ simulator - [...] ============
get_filename_component(SIMULATOR_SRC_BIN_NAME ${CMAKE_CURRENT_LIST_DIR} NAME_WE)
set(SIMULATOR_SRC_BIN_NAME "${SIMULATOR_SRC_BIN_NAME}-cli")
EchoWithColor(COLOR GREEN "-- Configure ${SIMULATOR_SRC_BIN_NAME} on ${CMAKE_CURRENT_LIST_DIR}")

aux_source_directory(${CMAKE_CURRENT_LIST_DIR} SIMULATOR_SRC_LIST)
set(EXECUTABLE_OUTPUT_PATH "${PROJECT_INSTALL_BAS_DIR}/tools/simulator/bin")
file(MAKE_DIRECTORY "${PROJECT_INSTALL_BAS_DIR}/tools/simulator/bin")

# special setting for sample
add_compiler_define(ATBUS_MACRO_MSG_LIMIT=65536)
include_directories("${ATFRAMEWORK_BASE_DIR}/service/atgateway/protocols"
    ${ATFRAMEWORK_SERVICE_COMPONENT_DIR}
    "${ATFRAMEWORK_BASE_DIR}/export/atgw_inner_v1_c")

# build libserver_frame.so
file(GLOB_RECURSE SRC_LIST
    ${CMAKE_CURRENT_LIST_DIR}/*.h
    ${CMAKE_CURRENT_LIST_DIR}/*.hpp
    ${CMAKE_CURRENT_LIST_DIR}/*.c
    ${CMAKE_CURRENT_LIST_DIR}/*.cpp
    ${CMAKE_CURRENT_LIST_DIR}/*.cc
)

source_group_by_dir(SRC_LIST)

# these service will use atgateway protocol
include_directories(
    ${CMAKE_CURRENT_LIST_DIR} ${PROJECT_SERVER_FRAME_INC_DIR} ${ATFRAMEWORK_SERVICE_GATEWAY_PROTOCOL_DIR}
    "${CMAKE_CURRENT_LIST_DIR}/libsimulator_uv"
)

# protocol hack
if (OPENSSL_FOUND)
    add_compiler_define(LIBATFRAME_ATGATEWAY_ENABLE_OPENSSL=1)
elseif(LIBRESSL_FOUND)
    add_compiler_define(LIBATFRAME_ATGATEWAY_ENABLE_LIBRESSL=1)
elseif(MBEDTLS_FOUND)
    add_compiler_define(LIBATFRAME_ATGATEWAY_ENABLE_MBEDTLS=1)
endif()

add_executable(${SIMULATOR_SRC_BIN_NAME}
    ${SRC_LIST} ${PROJECT_3RD_PARTY_SRC_LIST}
)

target_link_libraries(${SIMULATOR_SRC_BIN_NAME}
    ${PROJECT_SERVER_FRAME_LIB_LINK}
    libatgw_inner_v1_c
    ${ATFRAMEWORK_ATBUS_LINK_NAME}
    ${ATFRAMEWORK_ATFRAME_UTILS_LINK_NAME}
    ${3RD_PARTY_PROTOBUF_LINK_NAME}
    ${3RD_PARTY_LIBUV_LINK_NAME}
    ${3RD_PARTY_CRYPT_LINK_NAME}
    ${COMPILER_OPTION_EXTERN_CXX_LIBS}
)
