function(project_copy_shared_lib)
    list(GET ARGV -1 DST)
    list(REMOVE_AT ARGV -1)
    foreach(SRC IN LISTS ARGV)
        #get_filename_component(SRC_REAL_PATH ${SRC} REALPATH)
        #get_filename_component(SRC_NAME ${SRC} NAME)
        #get_filename_component(SRC_REAL_NAME ${SRC_REAL_PATH} NAME)
        file(
            COPY ${SRC}
            DESTINATION ${DST}
            FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )

        #if (NOT ${SRC_REAL_NAME} STREQUAL ${SRC_NAME})
        #        file(RENAME "${DST}/${SRC_REAL_NAME}" "${DST}/${SRC_NAME}")
        #    endif()
    endforeach()
endfunction()

set(LOG_WRAPPER_CATEGORIZE_SIZE 16 CACHE STRING "日志分类个数限制")
add_compiler_define(LOG_WRAPPER_CATEGORIZE_SIZE=${LOG_WRAPPER_CATEGORIZE_SIZE})

