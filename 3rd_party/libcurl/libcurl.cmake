
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
include_directories(${3RD_PARTY_LIBCURL_INC_DIR})
