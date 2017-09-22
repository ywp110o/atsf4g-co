
# =========== 3rdparty libcurl ==================
if(NOT 3RD_PARTY_RAPIDJSON_BASE_DIR)
    set (3RD_PARTY_RAPIDJSON_BASE_DIR ${CMAKE_CURRENT_LIST_DIR})
endif()

set (3RD_PARTY_RAPIDJSON_REPO_DIR "${3RD_PARTY_RAPIDJSON_BASE_DIR}/repo")
set (3RD_PARTY_RAPIDJSON_VERSION master)

if (Rapidjson_ROOT)
    set(RAPIDJSON_ROOT ${Rapidjson_ROOT})
endif()

find_package(Rapidjson)
if(NOT Rapidjson_FOUND)
    if(NOT EXISTS ${3RD_PARTY_RAPIDJSON_BASE_DIR})
        message(STATUS "mkdir 3RD_PARTY_RAPIDJSON_BASE_DIR=${3RD_PARTY_RAPIDJSON_BASE_DIR}")
        file(MAKE_DIRECTORY ${3RD_PARTY_RAPIDJSON_BASE_DIR})
    endif()

    if(NOT EXISTS ${3RD_PARTY_RAPIDJSON_REPO_DIR})
        find_package(Git)
        if(NOT GIT_FOUND)
            message(FATAL_ERROR "git not found")
        endif()

        file(RELATIVE_PATH 3RD_PARTY_RAPIDJSON_GIT_SUBMODULE_PATH ${CMAKE_SOURCE_DIR} ${3RD_PARTY_RAPIDJSON_REPO_DIR})
        execute_process(COMMAND ${GIT_EXECUTABLE} clone -b ${3RD_PARTY_RAPIDJSON_VERSION} --depth=1 "https://github.com/Tencent/rapidjson.git" ${3RD_PARTY_RAPIDJSON_REPO_DIR}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
    endif()

    set(Rapidjson_ROOT ${3RD_PARTY_RAPIDJSON_REPO_DIR})
    set(RAPIDJSON_ROOT ${3RD_PARTY_RAPIDJSON_REPO_DIR})
    set (3RD_PARTY_RAPIDJSON_ROOT_DIR ${3RD_PARTY_RAPIDJSON_REPO_DIR})
    find_package(Rapidjson)
else()
    set(3RD_PARTY_RAPIDJSON_ROOT_DIR ${RAPIDJSON_ROOT})
endif()

if(Rapidjson_FOUND)
    EchoWithColor(COLOR GREEN "-- Dependency: rapidjson found.(${Rapidjson_INCLUDE_DIRS})")
else()
    EchoWithColor(COLOR RED "-- Dependency: rapidjson is required")
    message(FATAL_ERROR "rapidjson not found")
endif()


set (3RD_PARTY_RAPIDJSON_INC_DIR ${Rapidjson_INCLUDE_DIRS})
include_directories(${3RD_PARTY_RAPIDJSON_INC_DIR})
