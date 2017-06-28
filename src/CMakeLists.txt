include_directories(${ATFRAMEWORK_SERVICE_COMPONENT_DIR})


include("${CMAKE_CURRENT_LIST_DIR}/server_frame/server_frame.cmake")

add_project_recurse(${CMAKE_CURRENT_LIST_DIR})