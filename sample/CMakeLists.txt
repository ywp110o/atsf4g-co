

EchoWithColor(COLOR GREEN "-- Configure ${CMAKE_CURRENT_LIST_DIR}")

# ============ sample - [...] ============
file(GLOB SAMPLE_BIN_LIST LIST_DIRECTORIES true "${CMAKE_CURRENT_LIST_DIR}/*")

foreach(SAMPLE_BIN_DIR IN LISTS SAMPLE_BIN_LIST)
    if(IS_DIRECTORY ${SAMPLE_BIN_DIR} AND EXISTS "${SAMPLE_BIN_DIR}/CMakeLists.txt")
        add_subdirectory(${SAMPLE_BIN_DIR})
    endif()
endforeach()
