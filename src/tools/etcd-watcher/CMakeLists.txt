# ============ etcd_loader_for_proxy - [...] ============
get_filename_component(TOOL_SRC_DIR_NAME ${CMAKE_CURRENT_LIST_DIR} NAME)
set(TOOL_SRC_BIN_NAME "${TOOL_SRC_DIR_NAME}-cli")
EchoWithColor(COLOR GREEN "-- Configure ${TOOL_SRC_BIN_NAME} on ${CMAKE_CURRENT_LIST_DIR}")

aux_source_directory(${CMAKE_CURRENT_LIST_DIR} SIMULATOR_SRC_LIST)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_INSTALL_BAS_DIR}/tools/${TOOL_SRC_DIR_NAME}/bin")
file(MAKE_DIRECTORY "${PROJECT_INSTALL_BAS_DIR}/tools/${TOOL_SRC_DIR_NAME}/bin")

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
    ${CMAKE_CURRENT_LIST_DIR}
    ${ATFRAMEWORK_SERVICE_COMPONENT_DIR}
)

link_directories(${CMAKE_LIBRARY_OUTPUT_DIRECTORY} ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY})

add_executable(${TOOL_SRC_BIN_NAME}
    ${SRC_LIST} ${PROJECT_3RD_PARTY_SRC_LIST}
)

target_link_libraries(${TOOL_SRC_BIN_NAME}
    ${PROJECT_LIB_LINK}
    ${ATFRAMEWORK_SERVICE_COMPONENT_LINK_NAME}
    ${ATFRAMEWORK_ATFRAME_UTILS_LINK_NAME}
    ${3RD_PARTY_LIBCURL_LINK_NAME}
    ${3RD_PARTY_LIBUV_LINK_NAME}
    ${3RD_PARTY_CRYPT_LINK_NAME}
    ${COMPILER_OPTION_EXTERN_CXX_LIBS}
)