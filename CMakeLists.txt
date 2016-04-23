cmake_minimum_required(VERSION 3.1.0)
cmake_policy(SET CMP0054 NEW)

project(libatf4g-co)

include("${CMAKE_SOURCE_DIR}/3rd_party/3rd_party.prepare.cmake")
include("${CMAKE_SOURCE_DIR}/atframework/atframework.prepare.cmake")

########################################################################
# CMake 模块 (递归包含模块, 带颜色输出模块, 平台检测模块)
set (PROJECT_CMAKE_MODULE_DIR "${ATFRAMEWORK_ATFRAME_UTILS_PKG_DIR}/project/cmake")
list(APPEND CMAKE_MODULE_PATH "${PROJECT_CMAKE_MODULE_DIR}/modules")


include("${PROJECT_CMAKE_MODULE_DIR}/modules/IncludeDirectoryRecurse.cmake")
include("${PROJECT_CMAKE_MODULE_DIR}/modules/EchoWithColor.cmake")
include("${PROJECT_CMAKE_MODULE_DIR}/modules/FindConfigurePackage.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/project/cmake/ProjectBuildOption.cmake")
include("${PROJECT_CMAKE_MODULE_DIR}/FindPlatform.cmake")
include("${PROJECT_CMAKE_MODULE_DIR}/ProjectTools.cmake")


#####################################################################
# 导入编译器和编译选项配置
include("${PROJECT_CMAKE_MODULE_DIR}/CompilerOption.cmake")
include("${PROJECT_CMAKE_MODULE_DIR}/TargetOption.cmake")
EchoWithColor(COLOR GREEN "-- Build Type: ${CMAKE_BUILD_TYPE}")

########################################################################
# 导入项目配置
## 导入所有 macro 定义
include_macro_recurse(${PROJECT_3RD_PARTY_ROOT_DIR})
# atframwork 内有依赖关系，所以不能直接导，要手动导入
include("${ATFRAMEWORK_BASE_DIR}/atframework.macro.cmake")

## 导入所有工程项目
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/atframework")
# add_project_recurse("${CMAKE_CURRENT_LIST_DIR}/src")

# if (PROJECT_ENABLE_SAMPLE)
#     add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/sample")
# endif()
#
# if (PROJECT_ENABLE_UNITTEST)
#     include ("${ATFRAMEWORK_ATFRAME_UTILS_PKG_DIR}/test/test.utils-macro.cmake")
#     add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/test")
# endif()
#
