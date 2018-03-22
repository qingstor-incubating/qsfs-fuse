cmake_minimum_required(VERSION 3.0)


#
# options
#
option (BUILD_PACKAGING    "Enable build of distribution packages using CPack." OFF)
option (BUILD_SHARED_LIBS  "Build shared libraries." OFF)
# to turn on, specify -DBUILD_TESTING=ON in cmake command line
option (BUILD_TESTING      "Enable build of the unit tests and their execution using CTest." OFF)
option (INSTALL_HEADERS    "Request installation of headers and other development files." OFF)
option (REGISTER_BUILD_DIR "Request entry of build directory in CMake's package registry." OFF)


#
# system info
#
execute_process(COMMAND uname -m OUTPUT_VARIABLE MACHINE_HARDWARE)
if (MACHINE_HARDWARE MATCHES "x86_64" OR MACHINE_HARDWARE MATCHES "x64"
    OR MACHINE_HARDWARE MATCHES "amd64")
  set(IS_MACH_64 TRUE CACHE BOOL "Machine hardware is 64-bit")
endif()

execute_process(COMMAND whoami OUTPUT_VARIABLE CURRENT_USER)
if(CURRENT_USER MATCHES "root")
  set (IS_ROOT TRUE CACHE BOOL "Current user is root")
endif()

if (IS_MACH_64)
  # when GCC is built as a 64-bit only compiler
  # this only affect configure step, not affect build step
  set(ENV{LIBRARY_PATH} "/usr/lib/x86_64-linux-gnu/:$LIBRARY_PATH"
      CACHE PATH "library path env variable")
endif()

execute_process(COMMAND head -1 /etc/issue OUTPUT_VARIABLE DISTROS_ISSUE)
string (STRIP "${DISTROS_ISSUE}" DISTROS_ISSUE)
string (FIND "${DISTROS_ISSUE}" " " POS1)
string (SUBSTRING "${DISTROS_ISSUE}" 0 ${POS1} OS_NAME)
string (REGEX MATCH "([0-9]+\\.[0-9]+)" OS_VERSION "${DISTROS_ISSUE}")
if (OS_NAME MATCHES "CentOS" OR OS_NAME MATCHES "RedHat")
  set(OS_NAME "el")
  string (REPLACE "." "_" OS_VERSION "${OS_VERSION}")
endif ()
string (CONCAT OS_NAME_VERSION ${OS_NAME} ${OS_VERSION})

#
# project information
#
set (PACKAGE_NAME          "qsfs")
set (PACKAGE_VERSION_MAJOR 1)
set (PACKAGE_VERSION_MINOR 0)
set (PACKAGE_VERSION_PATCH 1)
set (PACKAGE_VERSION       ${PACKAGE_VERSION_MAJOR}.${PACKAGE_VERSION_MINOR}.${PACKAGE_VERSION_PATCH})
set (PACKAGE_RELEASE       1)
set (PACKAGE_STRING        "${QSFS_NAME} ${QSFS_VERSION}")
set (PACKAGE_TARNAME       "${QSFS_NAME}-${QSFS_VERSION}")
set (PACKAGE_BUGREPORT     "https://github.com/yunify/qsfs-fuse/issues")
set (PACKAGE_DESCRIPTION   "A FUSE based filesystem allows you to mount a qingstor bucket in Linux.")
set (PACKAGE_URL           "https://github.com/yunify/qsfs-fuse")

string (TOUPPER "${PACKAGE_NAME}"       PACKAGE_PREFIX)

project (${PACKAGE_NAME})
message (STATUS "Building project version: ${PACKAGE_VERSION}")

#
# directories
#
# qsfs source directory
set (QSFS_SOURCE_DIR ${CMAKE_SOURCE_DIR}/src
     CACHE PATH "qsfs source directory")
set (THIRD_PARTY_SOURCE_DIR ${CMAKE_SOURCE_DIR}/third_party
     CACHE PATH "third party source directory")


#
# configure version
#
if (EXISTS ${QSFS_SOURCE_DIR}/configure/VersionConfig.h)
    FILE(REMOVE ${QSFS_SOURCE_DIR}/configure/VersionConfig.h)
endif ()
configure_file (cmake/templates/VersionConfig.h.in
                "${QSFS_SOURCE_DIR}/configure/VersionConfig.h")


# 
# dependencies
#
find_package (Threads REQUIRED)
if (NOT Threads_FOUND)
message (FATAL_ERROR "Could not find pthread. Check the log file"
"\n\t${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeError.log")
endif ()

# Not call find_package to check dependencies, instead
# download and install dependencies as static libraries
# under ./third_party/install
set (EXTERNAL_PROJECT_INSTALL_PREFIX ${CMAKE_SOURCE_DIR}/third_party/install
     CACHE PATH "Installation prefix for dependencies")
if (NOT googletest_INSTALLED)
  include (cmake/gtest.cmake)
endif()

if(NOT googleflags_INSTALLED)
  include (cmake/gflags.cmake)
endif()

if(NOT googlelog_INSTALLED)
  include(cmake/glog.cmake)
endif()

# qingstor sdk static libraries is not available now,
# so its shared libraries will be installed under /usr/local/lib
if (NOT qingstorsdk_INSTALLED)
  include (cmake/qingstorsdk.cmake)
endif()

#
# linter target (make lint)
#
add_custom_target (lint COMMAND ${CMAKE_COMMAND} -P
                   ${CMAKE_SOURCE_DIR}/cmake/lint.cmake)


#
# compile set up
#
# As we build gflags as a subproject and gflags is built as static single-threaded
# library by default. As glog depend on the gflags library and should link to gfalgs
# target, and this requires position-independent code.
# As not all systems support '-shared' option to produce a shared object
# which can then be linked with other object to form an executable.
# For predictable results, explicitly specify the -fPIC option.
# For example, on Debain (gcc 6.3.0), without explicit set -fPIC, cmake
# raise error: 
#   EXPORT glog-target requires target gflags_nothreads_static 
#   which is not in export set.
add_compile_options (-fPIC)

# add_compile_options (-Wall)  # for dev
# disable warnings for boost, this is only for gcc 4.8 or later
# add_compile_options (-Wno-unused-local-typedefs -Wno-delete-non-virtual-dtor)
# do not specify -g, instead specify -DCMAKE_BUILD_TYPE=Debug/Release in command line
# add_compile_options (-g -o0 -fno-inline)
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  set (CMAKE_INSTALL_DO_STRIP 0)
endif()

# fuse need to uses 64 bit off_t *
add_compile_options(-D_FILE_OFFSET_BITS=64)


#
# set up include/link directories
#
# Where qsfs's header files can be found
include_directories (${QSFS_SOURCE_DIR})

# Where boost files can be found
include_directories(${THIRD_PARTY_SOURCE_DIR}/boost_1_49_0)
# Where boost lib can be found
if (IS_MACH_64)
  # link to boost lib built on centos5.8-64bit with gcc4.1.2
  # and boost lib built on macOS10.12.5
  link_directories(${THIRD_PARTY_SOURCE_DIR}/boost_1_49_0/lib64)
else (IS_MACH_64)
  # link to boost lib built on centos5.8-32bitwith gcc4.1.2
  link_directories(${THIRD_PARTY_SOURCE_DIR}/boost_1_49_0/lib)
endif ()

# other dependencies (glog/gtest) built with source code and
# installed under third-party/install in cmake configure.
# Where dependencies's header files can be found.
include_directories(${EXTERNAL_PROJECT_INSTALL_PREFIX}/include)
# Where dependencies's targets can be found.
# On ubuntu x86_64 all libs get installed under lib
link_directories(${EXTERNAL_PROJECT_INSTALL_PREFIX}/lib)
if (IS_MACH_64 AND NOT APPLE)
  # On Centos x86_64 glog/gtest libs get installed under lib64
  link_directories(${EXTERNAL_PROJECT_INSTALL_PREFIX}/lib64)
endif()

# test on centos6.8 seems not include following default path
link_directories(/usr/local/lib)


#
# Make sure that the required libraries are always found independent 
# from LD_LIBRARY_PATH and the install location
#
# use, i.e. don't skip the full RPATH for the build tree
SET(CMAKE_SKIP_BUILD_RPATH  FALSE)

# when building, don't use the install RPATH already
# (but later on when installing)
SET(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE) 

# the RPATH to be used when installing
SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")

# add the automatically determined parts of the RPATH
# which point to directories outside the build tree to the install RPATH
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# the RPATH to be used when installing, but only if it's not a system directory
LIST(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${CMAKE_INSTALL_PREFIX}/lib" isSystemDir)
IF("${isSystemDir}" STREQUAL "-1")
   SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
ENDIF("${isSystemDir}" STREQUAL "-1")


#
# installation
#
# directories vars should be put before include directories
set (INSTALL_RUNTIME_DIR bin CACHE PATH "Installation directory for executables")
set (INSTALL_LIBRARY_DIR lib CACHE PATH "Installation directory for libraries")
set (INSTALL_INCLUDE_DIR include CACHE PATH "Installation directory for header files")
set (INSTALL_CMAKE_DIR  ${INSTALL_LIBRARY_DIR}/cmake/${PROJECT_NAME} CACHE PATH 
     "Installation directory for CMake files")
set (EXPORT_NAME ${PROJECT_NAME}-targets)
set (INSTALL_PREFIX_REL2CONFIG_DIR .)
file (RELATIVE_PATH INSTALL_PREFIX_REL2CONFIG_DIR
      "${CMAKE_INSTALL_PREFIX}/${INSTALL_CMAKE_DIR}" "${CMAKE_INSTALL_PREFIX}")

# for build tree
configure_file (cmake/templates/config.cmake.in
                "${PROJECT_BINARY_DIR}/${PROJECT_NAME}-config.cmake" @ONLY)
# for install tree
configure_file (cmake/templates/config.cmake.in
                "${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/${PROJECT_NAME}-config.cmake" @ONLY)
# for both
configure_file (cmake/templates/version.cmake.in 
                "${PROJECT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake" @ONLY)

if (INSTALL_HEADERS)
  install (FILES
    "${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/${PROJECT_NAME}-config.cmake"
    "${PROJECT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake"
    DESTINATION "${INSTALL_CMAKE_DIR}" COMPONENT dev
  )
  install (
    EXPORT ${EXPORT_NAME}
    DESTINATION "${INSTALL_CMAKE_DIR}" COMPONENT dev
  )
endif ()

# support direct use of build tree
export (TARGETS ${TARGETS} FILE "${PROJECT_BINARY_DIR}/${EXPORT_NAME}.cmake")
if (REGISTER_BUILD_DIR)
  export (PACKAGE ${PACKAGE_NAME})
endif ()


#
# uninstall
#
include(cmake/UninstallProject.cmake)
setup_uninstall_project(${PROJECT_NAME})

if(TARGET uninstall_${PROJECT_NAME})
  set (UNINSTALL_ALL ${UNINSTALL_ALL} uninstall_${PROJECT_NAME})
  set (UNINSTALL_DEFAULT ${UNINSTALL_DEFAULT} uninstall_${PROJECT_NAME})
endif()

if(TARGET uninstall_qingstorsdk)
  set (UNINSTALL_ALL ${UNINSTALL_ALL} uninstall_qingstorsdk)
  set (UNINSTALL_DEFAULT ${UNINSTALL_DEFAULT} uninstall_qingstorsdk)
endif()

if(TARGET uninstall_googletest)
  set (UNINSTALL_ALL ${UNINSTALL_ALL} uninstall_googletest)
endif()

if(TARGET uninstall_googlelog)
  set (UNINSTALL_ALL ${UNINSTALL_ALL} uninstall_googlelog)
endif()

if(TARGET uninstall_googleflags)
  set (UNINSTALL_ALL ${UNINSTALL_ALL} uninstall_googleflags)
endif()

# uninstall only uninstall qsfs and qingstor sdk, as they are
# installed under /usr/local
add_custom_target(
  uninstall DEPENDS ${UNINSTALL_DEFAULT}
)

add_custom_target(
  uninstall_all DEPENDS ${UNINSTALL_ALL}
)


#
# subdirectories
#
add_subdirectory (src)


#
# testing
#
if (BUILD_TESTING)
  include(CTest)
  enable_testing ()
  add_subdirectory(test)
endif (BUILD_TESTING)


#
# packaging
#
if (BUILD_PACKAGING)
  include (InstallRequiredSystemLibraries)

  # unix
  set (PACKAGE_GENERATOR        "DEB;RPM")
  set (PACKAGE_SOURCE_GENERATOR "TGZ;ZIP")

  # used package generators
  set (CPACK_GENERATOR        "${PACKAGE_GENERATOR}"        CACHE STRING "List of binary package generators (CPack).")
  set (CPACK_SOURCE_GENERATOR "${PACKAGE_SOURCE_GENERATOR}" CACHE STRING "List of source package generators (CPack).")
  mark_as_advanced (CPACK_GENERATOR CPACK_SOURCE_GENERATOR)

  # common package information
  set (CPACK_PACKAGE_VENDOR              "yunify")
  set (CPACK_PACKAGE_CONTACT             "jimhuang@yunify.com")
  set (CPACK_PACKAGE_NAME                "${PACKAGE_NAME}")
  set (CPACK_PACKAGE_VERSION             "${PACKAGE_VERSION}")
  set (CPACK_PACKAGE_VERSION_MAJOR       "${PACKAGE_VERSION_MAJOR}")
  set (CPACK_PACKAGE_VERSION_MINOR       "${PACKAGE_VERSION_MINOR}")
  set (CPACK_PACKAGE_VERSION_PATCH       "${PACKAGE_VERSION_PATCH}")
  set (CPACK_PACKAGE_RELEASE             "${PACKAGE_RELEASE}")
  set (CPACK_OS_NAME_VERSION             "${OS_NAME_VERSION}")  # cpack var must begin with CPACK
  set (CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PACKAGE_DESCRIPTION}")
  set (CPACK_RESOURCE_FILE_WELCOME       "${PROJECT_SOURCE_DIR}/README.md")
  set (CPACK_RESOURCE_FILE_LICENSE       "${PROJECT_SOURCE_DIR}/COPYING")
  set (CPACK_PACKAGE_DESCRIPTION_FILE    "${PROJECT_SOURCE_DIR}/README.md")
  set (CPACK_INSTALL_PREFIX              "${CMAKE_INSTALL_PREFIX}")
  set (CPACK_PACKAGING_INSTALL_PREFIX    "${CMAKE_INSTALL_PREFIX}")
  set (CPACK_OUTPUT_FILE_PREFIX          packages)
  set (CPACK_PACKAGE_RELOCATABLE         TRUE)
  set (CPACK_MONOLITHIC_INSTALL          TRUE)

  # RPM package information -- used in cmake/package.cmake.in also for DEB
  # following variables must defined here instead of cmake/package.cmake.in
  set (CPACK_RPM_PACKAGE_GROUP   "Applications/System")
  set (CPACK_RPM_PACKAGE_LICENSE "Apache")
  set (CPACK_RPM_PACKAGE_URL     "${PACKAGE_URL}")
  set (CPACK_RPM_PACKAGE_RELEASE "${CPACK_PACKAGE_RELEASE}")
  set (CPACK_RPM_CHANGELOG_FILE  "${PROJECT_SOURCE_DIR}/ChangeLog")
  if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    # to prevent rpmbuild strip debug symbols
    set (CPACK_RPM_SPEC_MORE_DEFINE "%define __spec_install_post /bin/true")
  endif()

  # unix system/architecture
  string (TOLOWER "${CMAKE_SYSTEM_NAME}" CPACK_SYSTEM_NAME)
  if (CMAKE_CXX_FLAGS MATCHES "-m32")
    set (CPACK_PACKAGE_ARCHITECTURE i386)
  else ()
    execute_process (
      COMMAND         dpkg --print-architecture
      RESULT_VARIABLE RV
      OUTPUT_VARIABLE CPACK_PACKAGE_ARCHITECTURE
    )
    if (RV EQUAL 0)
      string (STRIP "${CPACK_PACKAGE_ARCHITECTURE}" CPACK_PACKAGE_ARCHITECTURE)
    else ()
      execute_process (
        COMMAND uname -m 
        OUTPUT_VARIABLE CPACK_PACKAGE_ARCHITECTURE
      )
      if (CPACK_PACKAGE_ARCHITECTURE MATCHES "x86_64")
        set (CPACK_PACKAGE_ARCHITECTURE x86_64)
      else ()
        set (CPACK_PACKAGE_ARCHITECTURE i386)
      endif ()
    endif ()
  endif ()

  # source package settings
  set (CPACK_SOURCE_TOPLEVEL_TAG      "source")
  set (CPACK_SOURCE_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}")
  set (CPACK_SOURCE_IGNORE_FILES      "/\\\\.git/;\\\\.swp$;\\\\.#;/#;\\\\.*~;cscope\\\\.*;/[Bb]uild[.+-_a-zA-Z0-9]*/")

  # default binary package settings
  set (CPACK_INCLUDE_TOPLEVEL_DIRECTORY TRUE)
  set (CPACK_PACKAGE_FILE_NAME          "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CPACK_SYSTEM_NAME}")
  if (CPACK_PACKAGE_ARCHITECTURE)
    set (CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_FILE_NAME}-${CPACK_PACKAGE_ARCHITECTURE}")
  endif ()

  # generator specific configuration file
  #
  # allow package maintainers to use their own configuration file
  # $ cmake -DCPACK_PROJECT_CONFIG_FILE:FILE=/path/to/package/config
  if (NOT CPACK_PROJECT_CONFIG_FILE)
    configure_file (
      cmake/templates/package.cmake.in
      "${PROJECT_BINARY_DIR}/${PACKAGE_NAME}-package.cmake" @ONLY
    )
    set (CPACK_PROJECT_CONFIG_FILE "${PROJECT_BINARY_DIR}/${PACKAGE_NAME}-package.cmake")
  endif ()

  include (CPack)

endif (BUILD_PACKAGING) # BUILD_PACKAGING
