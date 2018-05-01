if (CMAKE_VERSION VERSION_LESS 3.2)
    set(UPDATE_DISCONNECTED_IF_AVAILABLE "")
else()
    # UPDATE_DISCONNECTED is 1 by default
    if (UPDATE_CONNECT)
        set(UPDATE_DISCONNECTED_IF_AVAILABLE "UPDATE_DISCONNECTED 0")
    endif()
endif()

include(cmake/DownloadInstallProject.cmake)

setup_download_project(PROJ    qingstorsdk
           GIT_REPOSITORY      https://github.com/yunify/qingstor-sdk-cpp.git
           GIT_TAG             master
           ${UPDATE_DISCONNECTED_IF_AVAILABLE}
)

# Download and 
download_project(qingstorsdk)

# Install
# propagate qingstor sdk headers installation option
set(INSTALL_SDK_HEADERS ${INSTALL_HEADERS} CACHE BOOL "" FORCE)
set (QS_STATICLIB true CACHE BOOL "" FORCE)
install_project(qingstorsdk ${EXTERNAL_PROJECT_INSTALL_PREFIX})

# Uninstall
include(cmake/UninstallProject.cmake)
setup_uninstall_project(qingstorsdk)