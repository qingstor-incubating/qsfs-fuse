if (CMAKE_VERSION VERSION_LESS 3.2)
    set(UPDATE_DISCONNECTED_IF_AVAILABLE "")
else()
    set(UPDATE_DISCONNECTED_IF_AVAILABLE "UPDATE_DISCONNECTED 1")
endif()

include(cmake/DownloadInstallProject.cmake)

setup_download_project(PROJ    qingstorsdk
           GIT_REPOSITORY      https://github.com/yunify/qingstor-sdk-cpp.git
           GIT_TAG             master
           ${UPDATE_DISCONNECTED_IF_AVAILABLE}
)

# Download and 
download_project(qingstorsdk)


if (BUILD_PACKAGING)
    # propagate qingstor sdk headers installation option
    set(INSTALL_SDK_HEADERS ${INSTALL_HEADERS} CACHE BOOL "" FORCE)
    add_subdirectory(${qingstorsdk_SOURCE_DIR})

    include_directories(${qingstorsdk_SOURCE_DIR}/include)
    link_directories(${CMAKE_BINARY_DIR}/build/qingstorsdk/source/lib)
    # as qingstorsdk is add as subdirectory of qsfs, so no need to uninstall individually
else(BUILD_PACKAGING)
    # Install
    # As qingstor static lib is not available for now, so we need to
    # install it on /usr/local in order to let make install linkage complete
    install_project(qingstorsdk ${CMAKE_INSTALL_PREFIX})

    # Uninstall
    include(cmake/UninstallProject.cmake)
    setup_uninstall_project(qingstorsdk)
endif(BUILD_PACKAGING)