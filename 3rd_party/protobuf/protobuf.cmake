
# =========== 3rdparty protobuf ==================
set (3RD_PARTY_PROTOBUF_BASE_DIR "${CMAKE_CURRENT_LIST_DIR}")
set (3RD_PARTY_PROTOBUF_PKG_DIR "${CMAKE_CURRENT_LIST_DIR}/pkg")

if(PROTOBUF_ROOT)
    set (3RD_PARTY_PROTOBUF_ROOT_DIR "${PROTOBUF_ROOT}")
else()
    set (3RD_PARTY_PROTOBUF_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/prebuilt/${PLATFORM_BUILD_PLATFORM_NAME}")
endif()

list(APPEND CMAKE_INCLUDE_PATH "${3RD_PARTY_PROTOBUF_ROOT_DIR}/include")
list(APPEND CMAKE_LIBRARY_PATH "${3RD_PARTY_PROTOBUF_ROOT_DIR}/lib")
list(APPEND CMAKE_PROGRAM_PATH "${3RD_PARTY_PROTOBUF_ROOT_DIR}/bin")
if (NOT WIN32 OR CYGWIN OR MINGW)
    FindConfigurePackage(
        PACKAGE Protobuf
        BUILD_WITH_CONFIGURE
        CONFIGURE_FLAGS "--enable-static=no"
        MAKE_FLAGS "-j4"
        PREBUILD_COMMAND "./autogen.sh"
        WORKING_DIRECTORY "${3RD_PARTY_PROTOBUF_PKG_DIR}"
        PREFIX_DIRECTORY "${3RD_PARTY_PROTOBUF_ROOT_DIR}"
        SRC_DIRECTORY_NAME "protobuf-3.3.0"
        TAR_URL "https://github.com/google/protobuf/releases/download/v3.3.0/protobuf-cpp-3.3.0.tar.gz"
    )
    # try again, cached vars wiil cause find failed.
    if (NOT PROTOBUF_FOUND OR NOT PROTOBUF_PROTOC_EXECUTABLE OR NOT Protobuf_INCLUDE_DIRS OR NOT Protobuf_LIBRARY)
        EchoWithColor(COLOR YELLOW "-- Dependency: Try to find protobuf libraries again")
        unset(Protobuf_LIBRARY)
        unset(Protobuf_PROTOC_LIBRARY)
        unset(Protobuf_INCLUDE_DIR)
        unset(Protobuf_PROTOC_EXECUTABLE)
        unset(Protobuf_LIBRARY_DEBUG)
        unset(Protobuf_PROTOC_LIBRARY_DEBUG)
        unset(Protobuf_LITE_LIBRARY)
        unset(Protobuf_LITE_LIBRARY_DEBUG)
        unset(Protobuf_LIBRARIES)
        unset(Protobuf_PROTOC_LIBRARIES)
        unset(Protobuf_LITE_LIBRARIES)
        find_package(Protobuf)
    endif()
else()
    find_package(Protobuf)
endif()

if(PROTOBUF_FOUND)
    EchoWithColor(COLOR GREEN "-- Dependency: Protobuf found.(${PROTOBUF_PROTOC_EXECUTABLE})")
    EchoWithColor(COLOR GREEN "-- Dependency: Protobuf include.(${Protobuf_INCLUDE_DIRS})")
    EchoWithColor(COLOR GREEN "-- Dependency: Protobuf libraries.(${Protobuf_LIBRARIES})")
else()
    EchoWithColor(COLOR RED "-- Dependency: Protobuf is required")
    message(FATAL_ERROR "Protobuf not found")
endif()

execute_process(COMMAND chmod +x "${PROTOBUF_PROTOC_EXECUTABLE}")

set (3RD_PARTY_PROTOBUF_INC_DIR ${PROTOBUF_INCLUDE_DIRS})
get_filename_component(3RD_PARTY_PROTOBUF_LIB_DIR ${Protobuf_LIBRARY} DIRECTORY)
set (3RD_PARTY_PROTOBUF_LINK_NAME ${Protobuf_LIBRARIES})
set (3RD_PARTY_PROTOBUF_BIN_PROTOC ${PROTOBUF_PROTOC_EXECUTABLE})

include_directories(${3RD_PARTY_PROTOBUF_INC_DIR})

file(GLOB 3RD_PARTY_PROTOBUF_ALL_LIB_FILES  "${3RD_PARTY_PROTOBUF_LIB_DIR}/*.so" "${3RD_PARTY_PROTOBUF_LIB_DIR}/*.so.*")
project_copy_shared_lib(${3RD_PARTY_PROTOBUF_ALL_LIB_FILES} ${PROJECT_INSTALL_SHARED_DIR})
