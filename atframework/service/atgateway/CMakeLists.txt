
set (ATSF4G_APP_NAME atgateway)
EchoWithColor(COLOR GREEN "-- Configure ${ATSF4G_APP_NAME} on ${CMAKE_CURRENT_LIST_DIR}")

# ============ atgateway ============

file(GLOB_RECURSE ATSF4G_APP_SRC_LIST
    *.cpp
    *.cc
    *.c
    *.cxx
    *.h
    *.hpp
)
source_group_by_dir(ATSF4G_APP_SRC_LIST)

# protocol hack
if (OPENSSL_FOUND)
    add_compiler_define(LIBATFRAME_ATGATEWAY_ENABLE_OPENSSL=1)
elseif(LIBRESSL_FOUND)
    add_compiler_define(LIBATFRAME_ATGATEWAY_ENABLE_LIBRESSL=1)
elseif(MBEDTLS_FOUND)
    add_compiler_define(LIBATFRAME_ATGATEWAY_ENABLE_MBEDTLS=1)
endif()

set(EXECUTABLE_OUTPUT_PATH "${PROJECT_INSTALL_BAS_DIR}/atframe/${ATSF4G_APP_NAME}/bin")

if (NOT EXISTS ${EXECUTABLE_OUTPUT_PATH})
    file(MAKE_DIRECTORY ${EXECUTABLE_OUTPUT_PATH})
endif()

if (MINGW)
    list(APPEND EXTENTION_LINK_LIB stdc++)
endif()

add_executable(${ATSF4G_APP_NAME} ${ATSF4G_APP_SRC_LIST} ${PROJECT_3RD_PARTY_SRC_LIST})
target_link_libraries(${ATSF4G_APP_NAME}
    ${PROJECT_LIB_LINK}
    ${ATFRAMEWORK_SERVICE_LINK_NAME}
    ${ATFRAMEWORK_ATAPP_LINK_NAME}
    ${ATFRAMEWORK_ATBUS_LINK_NAME}
    ${ATFRAMEWORK_ATFRAME_UTILS_LINK_NAME}
    ${3RD_PARTY_LIBUV_LINK_NAME}
    ${3RD_PARTY_CRYPT_LINK_NAME}
    ${EXTENTION_LINK_LIB}
)
