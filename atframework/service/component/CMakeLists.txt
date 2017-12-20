
set (ATSF4G_APP_NAME ${ATFRAMEWORK_SERVICE_COMPONENT_LINK_NAME})
EchoWithColor(COLOR GREEN "-- Configure ${ATSF4G_APP_NAME} on ${CMAKE_CURRENT_LIST_DIR}")

configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/config/atframe_services_build_feature.h.in"
    "${CMAKE_CURRENT_LIST_DIR}/config/atframe_services_build_feature.h"
    @ONLY
)

# ============ atproxy ============

file(GLOB_RECURSE ATSF4G_APP_SRC_LIST
    *.cpp
    *.cc
    *.c
    *.cxx
    *.h
    *.hpp
)
source_group_by_dir(ATSF4G_APP_SRC_LIST)

add_library(${ATSF4G_APP_NAME} STATIC ${ATSF4G_APP_SRC_LIST})

install(TARGETS ${ATSF4G_APP_NAME}
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib${PLATFORM_SUFFIX}
    ARCHIVE DESTINATION lib${PLATFORM_SUFFIX}
)
