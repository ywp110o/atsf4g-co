
set (ATSF4G_APP_NAME libatservice_component)
EchoWithColor(COLOR GREEN "-- Configure ${ATSF4G_APP_NAME} on ${CMAKE_CURRENT_LIST_DIR}")

if (OPENSSL_FOUND)
    set(LIBATFRAME_ATGATEWAY_ENABLE_OPENSSL 1)
elseif(LIBRESSL_FOUND)
    set(LIBATFRAME_ATGATEWAY_ENABLE_LIBRESSL 1)
elseif(MBEDTLS_FOUND)
    set(LIBATFRAME_ATGATEWAY_ENABLE_MBEDTLS 1)
endif()

configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/config/atframe_services_build_feature.h.in"
    "${CMAKE_CURRENT_LIST_DIR}/config/atframe_services_build_feature.h"
    @ONLY
)

# ============ atproxy ============

file(GLOB_RECURSE ATSF4G_APP_SRC_LIST RELATIVE ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/*.cpp
    ${CMAKE_CURRENT_LIST_DIR}/*.cc
    ${CMAKE_CURRENT_LIST_DIR}/*.c
    ${CMAKE_CURRENT_LIST_DIR}/*.cxx
    ${CMAKE_CURRENT_LIST_DIR}/*.h
    ${CMAKE_CURRENT_LIST_DIR}/*.hpp
)

add_library(${ATSF4G_APP_NAME} ${ATSF4G_APP_SRC_LIST})

install(TARGETS ${ATSF4G_APP_NAME}
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib${PLATFORM_SUFFIX}
    ARCHIVE DESTINATION lib${PLATFORM_SUFFIX}
)
