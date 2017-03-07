
# =========== 3rdparty flatbuffer ==================
if(NOT 3RD_PARTY_FLATBUFFER_BASE_DIR)
    set (3RD_PARTY_FLATBUFFER_BASE_DIR ${CMAKE_CURRENT_LIST_DIR})
endif()

set (3RD_PARTY_FLATBUFFER_REPO_DIR "${3RD_PARTY_FLATBUFFER_BASE_DIR}/repo")
set (3RD_PARTY_FLATBUFFER_VERSION v1.6.0)
set (3RD_PARTY_FLATBUFFER_HEADER "${3RD_PARTY_FLATBUFFER_REPO_DIR}/include/flatbuffers/flatbuffers.h")

if(NOT EXISTS ${3RD_PARTY_FLATBUFFER_REPO_DIR} OR NOT EXISTS ${3RD_PARTY_FLATBUFFER_HEADER})
    if (NOT EXISTS "${3RD_PARTY_FLATBUFFER_REPO_DIR}/include/flatbuffers")
        file(MAKE_DIRECTORY "${3RD_PARTY_FLATBUFFER_REPO_DIR}/include/flatbuffers")
    endif()

    message(STATUS "start to download flatbuffers")
    FindConfigurePackageDownloadFile("https://raw.githubusercontent.com/google/flatbuffers/${3RD_PARTY_FLATBUFFER_VERSION}/include/flatbuffers/flatbuffers.h" ${3RD_PARTY_FLATBUFFER_HEADER})
endif()

if (NOT EXISTS ${3RD_PARTY_FLATBUFFER_HEADER})
    EchoWithColor(COLOR RED "-- Dependency: Flatbuffer is required")
    message(FATAL_ERROR "Flatbuffer not found")
endif()

set (3RD_PARTY_FLATBUFFER_INC_DIR "${3RD_PARTY_FLATBUFFER_REPO_DIR}/include")
set (3RD_PARTY_FLATBUFFER_SRC_DIR "${3RD_PARTY_FLATBUFFER_REPO_DIR}/src")

include_directories(${3RD_PARTY_FLATBUFFER_INC_DIR})
# list(APPEND PROJECT_3RD_PARTY_SRC_LIST "${3RD_PARTY_FLATBUFFER_SRC_DIR}/flathash.cpp")
