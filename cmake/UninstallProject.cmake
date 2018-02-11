function(setup_uninstall_project PROJECT)
    set(PROJ_UNINSTALL_CMAKE_DIR ${${PROJECT}_BINARY_DIR})

    if (NOT TARGET uninstall_${PROJECT})
        if(EXISTS ${PROJ_UNINSTALL_CMAKE_DIR}/cmake_uninstall.cmake)
            FILE(REMOVE ${PROJ_UNINSTALL_CMAKE_DIR}/cmake_uninstall.cmake)
        endif()
        configure_file (
            "cmake/templates/cmake_uninstall.cmake.in"
            "${PROJ_UNINSTALL_CMAKE_DIR}/cmake_uninstall.cmake" @ONLY)
        add_custom_target(uninstall_${PROJECT}
            COMMAND ${CMAKE_COMMAND} -P "${PROJ_UNINSTALL_CMAKE_DIR}/cmake_uninstall.cmake")
    endif ()
endfunction()