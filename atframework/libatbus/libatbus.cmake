
# =========== libatapp ==================
set (ATFRAMEWORK_ATAPP_BASE_DIR ${CMAKE_CURRENT_LIST_DIR})

if (LIBATAPP_ROOT)
    set (ATFRAMEWORK_ATAPP_PKG_DIR ${LIBATAPP_ROOT})
else()
    set (ATFRAMEWORK_ATAPP_PKG_DIR "${ATFRAMEWORK_ATAPP_BASE_DIR}/repo")
    if(NOT EXISTS ${ATFRAMEWORK_ATAPP_PKG_DIR})
        find_package(Git)
        execute_process(COMMAND ${GIT_EXECUTABLE} clone "https://github.com/atframework/libatapp.git" ${ATFRAMEWORK_ATBUS_PKG_DIR}
            WORKING_DIRECTORY ${ATFRAMEWORK_ATAPP_BASE_DIR}
        )
    endif()
endif()

set (ATFRAMEWORK_ATAPP_INC_DIR "${ATFRAMEWORK_ATAPP_PKG_DIR}/include")
set (ATFRAMEWORK_ATAPP_SRC_DIR "${ATFRAMEWORK_ATAPP_PKG_DIR}/src")
set (ATFRAMEWORK_ATAPP_LINK_NAME atapp)
