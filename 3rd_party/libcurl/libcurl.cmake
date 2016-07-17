
# =========== 3rdparty libcurl ==================
if(NOT 3RD_PARTY_LIBCURL_BASE_DIR)
    set (3RD_PARTY_LIBCURL_BASE_DIR ${CMAKE_CURRENT_LIST_DIR})
endif()

set (3RD_PARTY_LIBCURL_PKG_DIR "${3RD_PARTY_LIBCURL_BASE_DIR}/pkg")

set (3RD_PARTY_LIBCURL_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/prebuilt/${PLATFORM_BUILD_PLATFORM_NAME}")

set (3RD_PARTY_LIBCURL_VERSION "7.49.1")
set (3RD_PARTY_LIBCURL_PKG_NAME "curl-${3RD_PARTY_LIBCURL_VERSION}.tar.gz")
set (3RD_PARTY_LIBCURL_SRC_URL "http://curl.haxx.se/download/${3RD_PARTY_LIBCURL_PKG_NAME}")

if (Libcurl_ROOT)
    set(LIBCURL_ROOT ${Libcurl_ROOT})
endif()

if (LIBCURL_ROOT)
    list(APPEND CMAKE_LIBRARY_PATH "${LIBCURL_ROOT}/lib${PLATFORM_SUFFIX}" "${LIBCURL_ROOT}/lib")
    list(APPEND CMAKE_INCLUDE_PATH "${LIBCURL_ROOT}/include")
endif()

find_package(CURL)

if(NOT CURL_FOUND)
    if(NOT EXISTS ${3RD_PARTY_LIBCURL_PKG_DIR})
        message(STATUS "mkdir 3RD_PARTY_LIBCURL_PKG_DIR=${3RD_PARTY_LIBCURL_PKG_DIR}")
        file(MAKE_DIRECTORY ${3RD_PARTY_LIBCURL_PKG_DIR})
    endif()

    if(NOT EXISTS "${3RD_PARTY_LIBCURL_PKG_DIR}/${3RD_PARTY_LIBCURL_PKG_NAME}")
        FindConfigurePackageDownloadFile(${3RD_PARTY_LIBCURL_SRC_URL} "${3RD_PARTY_LIBCURL_PKG_DIR}/${3RD_PARTY_LIBCURL_PKG_NAME}")
    endif()

    find_program(TAR_EXECUTABLE tar)
    if(APPLE)
        execute_process(COMMAND ${TAR_EXECUTABLE} -xvf ${3RD_PARTY_LIBCURL_PKG_NAME}
            WORKING_DIRECTORY ${3RD_PARTY_LIBCURL_PKG_DIR}
        )
    else()
        execute_process(COMMAND ${TAR_EXECUTABLE} -axvf ${3RD_PARTY_LIBCURL_PKG_NAME}
            WORKING_DIRECTORY ${3RD_PARTY_LIBCURL_PKG_DIR}
        )
    endif()

    set(LIBCURL_ROOT ${3RD_PARTY_LIBCURL_ROOT_DIR})
    execute_process(COMMAND ./configure "--prefix=${3RD_PARTY_LIBCURL_ROOT_DIR}" --with-pic=yes
        WORKING_DIRECTORY "${3RD_PARTY_LIBCURL_PKG_DIR}/${3RD_PARTY_LIBCURL_PKG_NAME}"
    )

    execute_process(COMMAND make -j4 install
        WORKING_DIRECTORY "${3RD_PARTY_LIBCURL_PKG_DIR}/${3RD_PARTY_LIBCURL_PKG_NAME}"
    )
    list(APPEND CMAKE_LIBRARY_PATH "${LIBCURL_ROOT}/lib${PLATFORM_SUFFIX}" "${LIBCURL_ROOT}/lib")
    list(APPEND CMAKE_INCLUDE_PATH "${LIBCURL_ROOT}/include")
    find_package(CURL)
endif()

if(CURL_FOUND)
    EchoWithColor(COLOR GREEN "-- Dependency: libcurl found.(${CURL_INCLUDE_DIRS}/${CURL_LIBRARIES})")
else()
    EchoWithColor(COLOR RED "-- Dependency: libcurl is required")
    message(FATAL_ERROR "libcurl not found")
endif()


set (3RD_PARTY_LIBCURL_INC_DIR ${CURL_INCLUDE_DIRS})
set (3RD_PARTY_LIBCURL_LINK_NAME ${CURL_LIBRARIES})
if(WIN32 OR MINGW)
    # curl has so many dependency libraries, so use dynamic library first
    string(REGEX REPLACE "\\.a" ".dll.a" 3RD_PARTY_LIBCURL_LINK_DYN_NAME ${3RD_PARTY_LIBCURL_LINK_NAME})
    if(3RD_PARTY_LIBCURL_LINK_DYN_NAME AND NOT ${3RD_PARTY_LIBCURL_LINK_DYN_NAME} STREQUAL ${3RD_PARTY_LIBCURL_LINK_NAME})
        if (EXISTS ${3RD_PARTY_LIBCURL_LINK_DYN_NAME})
            set (3RD_PARTY_LIBCURL_LINK_NAME ${3RD_PARTY_LIBCURL_LINK_DYN_NAME})
        endif()
    endif()
endif()
include_directories(${3RD_PARTY_LIBCURL_INC_DIR})

set(3RD_PARTY_LIBCURL_TEST_SRC "#include <curl/curl.h>
#include <stdio.h>

int main () {
    curl_global_init(CURL_GLOBAL_ALL)\;
    printf(\"libcurl version: %s\", LIBCURL_VERSION)\;
    return 0\; 
}")

file(WRITE "${CMAKE_BINARY_DIR}/try_run_libcurl_test.c" ${3RD_PARTY_LIBCURL_TEST_SRC})

try_run(3RD_PARTY_LIBCURL_TRY_RUN_RESULT 3RD_PARTY_LIBCURL_TRY_COMPILE_RESULT
    ${CMAKE_BINARY_DIR} "${CMAKE_BINARY_DIR}/try_run_libcurl_test.c"
    LINK_LIBRARIES ${3RD_PARTY_LIBCURL_LINK_NAME}
    COMPILE_OUTPUT_VARIABLE 3RD_PARTY_LIBCURL_TRY_COMPILE_DYN_MSG
    RUN_OUTPUT_VARIABLE 3RD_PARTY_LIBCURL_TRY_RUN_OUT
)

if (NOT 3RD_PARTY_LIBCURL_TRY_COMPILE_RESULT)
    EchoWithColor(COLOR YELLOW "-- Libcurl: Dynamic symbol test in ${3RD_PARTY_LIBCURL_LINK_NAME} failed, try static symbols")

    if ( NOT MSVC )
        set(3RD_PARTY_LIBCURL_STATIC_DEF -DCURL_STATICLIB)
    else()
        set(3RD_PARTY_LIBCURL_STATIC_DEF /D CURL_STATICLIB)
    endif()
    try_run(3RD_PARTY_LIBCURL_TRY_RUN_RESULT 3RD_PARTY_LIBCURL_TRY_COMPILE_RESULT
        ${CMAKE_BINARY_DIR} "${CMAKE_BINARY_DIR}/try_run_libcurl_test.c"
        COMPILE_DEFINITIONS ${3RD_PARTY_LIBCURL_STATIC_DEF}
        LINK_LIBRARIES ${3RD_PARTY_LIBCURL_LINK_NAME}
        COMPILE_OUTPUT_VARIABLE 3RD_PARTY_LIBCURL_TRY_COMPILE_STA_MSG
        RUN_OUTPUT_VARIABLE 3RD_PARTY_LIBCURL_TRY_RUN_OUT
    )

    if (NOT 3RD_PARTY_LIBCURL_TRY_COMPILE_RESULT)
        message(STATUS ${3RD_PARTY_LIBCURL_TRY_COMPILE_DYN_MSG})
        message(STATUS ${3RD_PARTY_LIBCURL_TRY_COMPILE_STA_MSG})
        message(FATAL_ERROR "Libcurl: try compile with ${3RD_PARTY_LIBCURL_LINK_NAME} failed")
    else()
        EchoWithColor(COLOR GREEN "-- Libcurl: use static symbols")
        message(STATUS ${3RD_PARTY_LIBCURL_TRY_RUN_OUT})
        add_compiler_define(CURL_STATICLIB)
    endif()
else()
    EchoWithColor(COLOR GREEN "-- Libcurl: use dynamic symbols")
    message(STATUS ${3RD_PARTY_LIBCURL_TRY_RUN_OUT})
endif()