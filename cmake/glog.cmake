if (CMAKE_VERSION VERSION_LESS 3.2)
    set(UPDATE_DISCONNECTED_IF_AVAILABLE "")
else()
    set(UPDATE_DISCONNECTED_IF_AVAILABLE "UPDATE_DISCONNECTED 1")
endif()

include(cmake/DownloadInstallProject.cmake)

# Use googlelog instead of glog to avoid override the CMAKE implict variables
# glog_SOURCE_DIR and glog_BINARY_DIR
setup_download_project(PROJ    googlelog
           GIT_REPOSITORY      https://github.com/google/glog.git
           GIT_TAG             master
           ${UPDATE_DISCONNECTED_IF_AVAILABLE}
)

# Download
download_project(googlelog)

# Install
install_project(googlelog ${EXTERNAL_PROJECT_INSTALL_PREFIX})

# Uninstall
include(cmake/UninstallProject.cmake)
setup_uninstall_project(googlelog)