
# =========== 3rdparty protobuf ==================
set (3RD_PARTY_PROTOBUF_BASE_DIR "${CMAKE_CURRENT_LIST_DIR}")
set (3RD_PARTY_PROTOBUF_PKG_DIR "${CMAKE_CURRENT_LIST_DIR}/pkg")
set (3RD_PARTY_PROTOBUF_VERSION "3.4.1")

if(PROTOBUF_ROOT)
    set (3RD_PARTY_PROTOBUF_ROOT_DIR "${PROTOBUF_ROOT}")
else()
    set (3RD_PARTY_PROTOBUF_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/prebuilt/${PLATFORM_BUILD_PLATFORM_NAME}")
endif()

list(APPEND CMAKE_INCLUDE_PATH "${3RD_PARTY_PROTOBUF_ROOT_DIR}/include")
list(APPEND CMAKE_LIBRARY_PATH "${3RD_PARTY_PROTOBUF_ROOT_DIR}/lib")
list(APPEND CMAKE_PROGRAM_PATH "${3RD_PARTY_PROTOBUF_ROOT_DIR}/bin")

# 如果要设置ABI规则（比如GCC使用老的符号规则:-D_GLIBCXX_USE_CXX11_ABI=0） 可以加在PROJECT_COMPILER_ADAPTOR_FLAGS里。但一定要保证编译protobuf和工程中所用的一致
if(NOT WIN32 AND NOT CYGWIN AND NOT MINGW) 
    set(3RD_PARTY_PROTOBUF_PIE -DCMAKE_C_FLAGS="-fPIC ${PROJECT_COMPILER_ADAPTOR_FLAGS}" -DCMAKE_CXX_FLAGS="-fPIC ${PROJECT_COMPILER_ADAPTOR_FLAGS}")
else()
    set(3RD_PARTY_PROTOBUF_PIE -DCMAKE_C_FLAGS="${PROJECT_COMPILER_ADAPTOR_FLAGS}" -DCMAKE_CXX_FLAGS="${PROJECT_COMPILER_ADAPTOR_FLAGS}")
endif()

if (NOT EXISTS ${3RD_PARTY_PROTOBUF_PKG_DIR})
    file(MAKE_DIRECTORY ${3RD_PARTY_PROTOBUF_PKG_DIR})
endif()

# MSVC 必须用静态库，而且会被用/MT编译。我们要把默认的/MD改为/MT
# 使用 /MD protobuf有时候会崩溃，/MT依赖较少不容易出问题
# 注意protobuf的 RelWithDebInfo 默认使用 /MT 而邮箱工程默认是 /MTd
if (MSVC)
    set (3RD_PARTY_PROTOBUF_BUILD_SHARED_LIBS -DBUILD_SHARED_LIBS=OFF)
    # add_compiler_define(PROTOBUF_USE_DLLS) # MSVC 使用动态库必须加这个选项
    foreach(flag_var
        CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
        CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
        if(${flag_var} MATCHES "/MD")
            string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
        endif(${flag_var} MATCHES "/MD")
    endforeach(flag_var)
#else ()
    # 其他情况使用默认值即可
    # set (3RD_PARTY_PROTOBUF_BUILD_SHARED_LIBS OFF)
endif ()


FindConfigurePackage(
    PACKAGE Protobuf
    BUILD_WITH_CMAKE
    CMAKE_FLAGS ${3RD_PARTY_PROTOBUF_PIE} -Dprotobuf_BUILD_TESTS=OFF ${3RD_PARTY_PROTOBUF_BUILD_SHARED_LIBS}
    WORKING_DIRECTORY "${3RD_PARTY_PROTOBUF_PKG_DIR}"
    PREFIX_DIRECTORY "${3RD_PARTY_PROTOBUF_ROOT_DIR}"
    SRC_DIRECTORY_NAME "protobuf-${3RD_PARTY_PROTOBUF_VERSION}"
    BUILD_DIRECTORY "${3RD_PARTY_PROTOBUF_PKG_DIR}/protobuf-${3RD_PARTY_PROTOBUF_VERSION}/build-obj-dir"
    PROJECT_DIRECTORY "${3RD_PARTY_PROTOBUF_PKG_DIR}/protobuf-${3RD_PARTY_PROTOBUF_VERSION}/cmake"
    TAR_URL "https://github.com/google/protobuf/releases/download/v${3RD_PARTY_PROTOBUF_VERSION}/protobuf-cpp-${3RD_PARTY_PROTOBUF_VERSION}.tar.gz"
    ZIP_URL "https://github.com/google/protobuf/releases/download/v${3RD_PARTY_PROTOBUF_VERSION}/protobuf-cpp-${3RD_PARTY_PROTOBUF_VERSION}.zip"
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

if(PROTOBUF_FOUND)
    EchoWithColor(COLOR GREEN "-- Dependency: Protobuf found.(${PROTOBUF_PROTOC_EXECUTABLE})")
    EchoWithColor(COLOR GREEN "-- Dependency: Protobuf include.(${Protobuf_INCLUDE_DIRS})")
else()
    EchoWithColor(COLOR RED "-- Dependency: Protobuf is required")
    message(FATAL_ERROR "Protobuf not found")
endif()

execute_process(COMMAND chmod +x "${PROTOBUF_PROTOC_EXECUTABLE}")

set (3RD_PARTY_PROTOBUF_INC_DIR ${PROTOBUF_INCLUDE_DIRS})
if (${CMAKE_BUILD_TYPE} STREQUAL "Debug" AND Protobuf_LIBRARY_DEBUG)
    get_filename_component(3RD_PARTY_PROTOBUF_LIB_DIR ${Protobuf_LIBRARY_DEBUG} DIRECTORY)
    get_filename_component(3RD_PARTY_PROTOBUF_BIN_DIR ${PROTOBUF_PROTOC_EXECUTABLE} DIRECTORY)
    set (3RD_PARTY_PROTOBUF_LINK_NAME ${Protobuf_LIBRARY_DEBUG})
    set (3RD_PARTY_PROTOBUF_LITE_LINK_NAME ${Protobuf_LITE_LIBRARY_DEBUG})
    EchoWithColor(COLOR GREEN "-- Dependency: Protobuf libraries.(${Protobuf_LIBRARY_DEBUG})")
    EchoWithColor(COLOR GREEN "-- Dependency: Protobuf lite libraries.(${Protobuf_LITE_LIBRARY_DEBUG})")
else()
    get_filename_component(3RD_PARTY_PROTOBUF_LIB_DIR ${Protobuf_LIBRARY} DIRECTORY)
    get_filename_component(3RD_PARTY_PROTOBUF_BIN_DIR ${PROTOBUF_PROTOC_EXECUTABLE} DIRECTORY)
    set (3RD_PARTY_PROTOBUF_LINK_NAME ${Protobuf_LIBRARY})
    set (3RD_PARTY_PROTOBUF_LITE_LINK_NAME ${Protobuf_LITE_LIBRARY})
    EchoWithColor(COLOR GREEN "-- Dependency: Protobuf libraries.(${Protobuf_LIBRARY})")
    EchoWithColor(COLOR GREEN "-- Dependency: Protobuf lite libraries.(${Protobuf_LITE_LIBRARY})")
endif()

set (3RD_PARTY_PROTOBUF_BIN_PROTOC ${PROTOBUF_PROTOC_EXECUTABLE})

include_directories(${3RD_PARTY_PROTOBUF_INC_DIR})

file(GLOB 3RD_PARTY_PROTOBUF_ALL_LIB_FILES 
    "${3RD_PARTY_PROTOBUF_LIB_DIR}/libprotobuf*.so"
    "${3RD_PARTY_PROTOBUF_LIB_DIR}/libprotobuf*.so.*"
    "${3RD_PARTY_PROTOBUF_LIB_DIR}/libprotobuf*.dll"
    "${3RD_PARTY_PROTOBUF_BIN_DIR}/libprotobuf*.so"
    "${3RD_PARTY_PROTOBUF_BIN_DIR}/libprotobuf*.so.*"
    "${3RD_PARTY_PROTOBUF_BIN_DIR}/libprotobuf*.dll"
)
project_copy_shared_lib(${3RD_PARTY_PROTOBUF_ALL_LIB_FILES} ${PROJECT_INSTALL_SHARED_DIR})
