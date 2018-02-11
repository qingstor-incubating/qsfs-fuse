

file(GLOB QSFS_BASE_HEADERS "base/*.h")
file(GLOB QSFS_CLIENT_HEADERS "client/*.h")
file(GLOB QSFS_CONFIGURE_HEADERS "configure/*.h")
file(GLOB QSFS_DATA_HEADERS "data/*.h")
file(GLOB QSFS_FILESYSTEM_HEADERS "filesystem/*.h")

file(GLOB QSFS_SRCS "*.cpp")
file(GLOB QSFS_BASE_SRCS "base/*.cpp")
file(GLOB QSFS_CLIENT_SRCS "client/*.cpp")
file(GLOB QSFS_CONFIGURE_SRCS "configure/*.cpp")
file(GLOB QSFS_DATA_SRCS "data/*.cpp")
file(GLOB QSFS_FILESYSTEM_SRCS "filesystem/*.cpp")
file(GLOB QSFS_SOURCES 
  ${QSFS_SRCS}
  ${QSFS_BASE_SRCS}
  ${QSFS_CLIENT_SRCS}
  ${QSFS_CONFIGURE_SRCS}
  ${QSFS_DATA_SRCS}
  ${QSFS_FILESYSTEM_SRCS}
  )

#
# main program
#
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
add_executable(
  ${PROJECT_NAME}
  ${QSFS_SOURCES}
  )
if (APPLE)
  target_link_libraries(${PROJECT_NAME} osxfuse osxboost_thread)
elseif (UNIX)
  target_link_libraries(${PROJECT_NAME} fuse boost_thread)
endif ()
target_link_libraries(${PROJECT_NAME} glog gflags ${CMAKE_THREAD_LIBS_INIT} qingstor)
set_target_properties(${PROJECT_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE CXX)

#
# install target
#
install (
  TARGETS ${PROJECT_NAME}
  EXPORT ${EXPORT_NAME}
  RUNTIME DESTINATION "${INSTALL_RUNTIME_DIR}" COMPONENT bin
)


#
# make libs referenced by unit test
#
# use OBJECT lib type, as we do not need to produce any library files

add_library(
  qsfsFileMetaData OBJECT
  data/FileMetaData.cpp
  data/FileMetaDataManager.cpp
  configure/Options.cpp
)

add_library(
  qsfsLogging OBJECT
  base/Logging.cpp
  base/LogLevel.cpp
  base/Utils.cpp
  base/StringUtils.cpp
  configure/Default.cpp
)

add_library(
  qsfsStream OBJECT
  data/IOStream.cpp
  data/StreamBuf.cpp
  data/StreamUtils.cpp
)

#
# install headers
#
if (INSTALL_HEADERS)
  install (
    FILES 
    ${QSFS_BASE_HEADERS} 
    DESTINATION ${INSTALL_INCLUDE_DIR}/${PROJECT_NAME}/base COMPONENT dev
    )

  install (
    FILES 
    ${QSFS_CLIENT_HEADERS} 
    DESTINATION ${INSTALL_INCLUDE_DIR}/${PROJECT_NAME}/client COMPONENT dev
    )

  install (
    FILES 
    ${QSFS_CONFIGURE_HEADERS} 
    DESTINATION ${INSTALL_INCLUDE_DIR}/${PROJECT_NAME}/configure COMPONENT dev
    )

  install (
    FILES 
    ${QSFS_DATA_HEADERS} 
    DESTINATION ${INSTALL_INCLUDE_DIR}/${PROJECT_NAME}/data COMPONENT dev
    )

  install (
    FILES 
    ${QSFS_FILESYSTEM_HEADERS} 
    DESTINATION ${INSTALL_INCLUDE_DIR}/${PROJECT_NAME}/filesystem COMPONENT dev
    )
endif ()