
# =========== libatbus ==================
set (ATFRAMEWORK_ATBUS_BASE_DIR ${CMAKE_CURRENT_LIST_DIR})
# make 3rd party detect
set (3RD_PARTY_LIBATBUS_BASE_DIR ${ATFRAMEWORK_ATBUS_BASE_DIR})

if (LIBATBUS_ROOT)
    set (ATFRAMEWORK_ATBUS_PKG_DIR ${LIBATBUS_ROOT})
else()
    set (ATFRAMEWORK_ATBUS_PKG_DIR "${ATFRAMEWORK_ATBUS_BASE_DIR}/repo")
    if(NOT EXISTS ${ATFRAMEWORK_ATBUS_PKG_DIR})
        find_package(Git)
        execute_process(COMMAND ${GIT_EXECUTABLE} clone "https://github.com/atframework/libatbus.git" ${ATFRAMEWORK_ATBUS_PKG_DIR}
            WORKING_DIRECTORY ${ATFRAMEWORK_ATBUS_BASE_DIR}
        )
    endif()
endif()

set (ATFRAMEWORK_ATBUS_INC_DIR "${ATFRAMEWORK_ATBUS_PKG_DIR}/include")
set (ATFRAMEWORK_ATBUS_SRC_DIR "${ATFRAMEWORK_ATBUS_PKG_DIR}/src")
set (ATFRAMEWORK_ATBUS_LINK_NAME atbus)

include_directories(${ATFRAMEWORK_ATBUS_INC_DIR})