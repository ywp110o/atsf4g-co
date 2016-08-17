# include directory for all service
include_directories(${ATFRAMEWORK_SERVICE_COMPONENT_DIR})

# base services
add_subdirectory(${ATFRAMEWORK_SERVICE_COMPONENT_DIR})
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/atproxy")
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/atgateway")