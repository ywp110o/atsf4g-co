# =========== 3rdparty libcopp ==================
set (3RD_PARTY_LIBCOPP_BASE_DIR ${CMAKE_CURRENT_LIST_DIR})
set (3RD_PARTY_LIBCOPP_PKG_DIR "${CMAKE_CURRENT_LIST_DIR}/pkg")
set (3RD_PARTY_LIBCOPP_REPO_DIR "${CMAKE_CURRENT_LIST_DIR}/repo")

if(LIBCOPP_ROOT)
    set (3RD_PARTY_LIBCOPP_ROOT_DIR ${LIBCOPP_ROOT})
else()
    set (3RD_PARTY_LIBCOPP_ROOT_DIR ${3RD_PARTY_LIBCOPP_REPO_DIR})
endif()

find_package(Libcopp)
if (Libcopp_FOUND)
    set (3RD_PARTY_LIBCOPP_INC_DIR ${Libcopp_INCLUDE_DIRS})
    set (3RD_PARTY_LIBCOPP_LINK_NAME ${Libcopp_LIBRARIES} ${Libcotask_LIBRARIES})

    include_directories(${3RD_PARTY_LIBCOPP_INC_DIR})
    EchoWithColor(COLOR GREEN "-- Dependency: libcopp prebuilt found.(inc=${Libcopp_INCLUDE_DIRS})")
elseif(EXISTS "${3RD_PARTY_LIBCOPP_REPO_DIR}/CMakeLists.txt")
    set (3RD_PARTY_LIBCOPP_INC_DIR "${3RD_PARTY_LIBCOPP_REPO_DIR}/include")
    set (3RD_PARTY_LIBCOPP_LINK_NAME libcopp libcotask)
    add_subdirectory(${3RD_PARTY_LIBCOPP_REPO_DIR})

    include_directories(${3RD_PARTY_LIBCOPP_INC_DIR})
    EchoWithColor(COLOR GREEN "-- Dependency: libcopp submodule found.(repository=${3RD_PARTY_LIBCOPP_REPO_DIR})")
else()
    EchoWithColor(COLOR RED "-- Dependency: libcopp is required")
    message(FATAL_ERROR "libcopp not found")
endif()
