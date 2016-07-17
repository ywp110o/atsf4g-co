
# =========== 3rdparty libcurl ==================
if(NOT 3RD_PARTY_RAPIDJSON_BASE_DIR)
    set (3RD_PARTY_RAPIDJSON_BASE_DIR ${CMAKE_CURRENT_LIST_DIR})
endif()

set (3RD_PARTY_RAPIDJSON_PKG_DIR "${3RD_PARTY_RAPIDJSON_BASE_DIR}/pkg")

set (3RD_PARTY_RAPIDJSON_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/prebuilt")

set (3RD_PARTY_RAPIDJSON_GIT_URL "https://github.com/miloyip/rapidjson.git")

if (Rapidjson_ROOT)
    set(RAPIDJSON_ROOT ${Rapidjson_ROOT})
endif()

find_package(Rapidjson)
if(NOT Rapidjson_FOUND)
    if(NOT EXISTS ${3RD_PARTY_RAPIDJSON_PKG_DIR})
        message(STATUS "mkdir 3RD_PARTY_RAPIDJSON_PKG_DIR=${3RD_PARTY_RAPIDJSON_PKG_DIR}")
        file(MAKE_DIRECTORY ${3RD_PARTY_RAPIDJSON_PKG_DIR})
    endif()

    find_package(Git)
    if(NOT GIT_FOUND)
        message(FATAL_ERROR "git not found")
    endif()

    execute_process(COMMAND ${GIT_EXECUTABLE} clone -b master ${3RD_PARTY_RAPIDJSON_GIT_URL} rapidjson
        WORKING_DIRECTORY ${3RD_PARTY_RAPIDJSON_PKG_DIR}
    )

    file(COPY "${3RD_PARTY_RAPIDJSON_PKG_DIR}/rapidjson/include" 
        DESTINATION "${3RD_PARTY_RAPIDJSON_ROOT_DIR}"
        USE_SOURCE_PERMISSIONS
    )

    set(Rapidjson_ROOT ${3RD_PARTY_RAPIDJSON_ROOT_DIR})
    find_package(Rapidjson)
endif()

if(Rapidjson_FOUND)
    EchoWithColor(COLOR GREEN "-- Dependency: rapidjson found.(${Rapidjson_INCLUDE_DIRS})")
else()
    EchoWithColor(COLOR RED "-- Dependency: rapidjson is required")
    message(FATAL_ERROR "rapidjson not found")
endif()


set (3RD_PARTY_RAPIDJSON_INC_DIR ${Rapidjson_INCLUDE_DIRS})
include_directories(${3RD_PARTY_RAPIDJSON_INC_DIR})
