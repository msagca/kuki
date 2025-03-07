project(test)
file(GLOB_RECURSE C_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/source/*.c")
file(GLOB_RECURSE CPP_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp")
add_executable(${PROJECT_NAME} ${C_SOURCES} ${CPP_SOURCES})
target_include_directories(
  ${PROJECT_NAME}
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(${PROJECT_NAME} PRIVATE engine gtest)
if(UNIX AND NOT APPLE)
  set_target_properties(${PROJECT_NAME} PROPERTIES
    INSTALL_RPATH "$ORIGIN"
    BUILD_WITH_INSTALL_RPATH TRUE
  )
endif()
if(WIN32)
  add_custom_command(
    TARGET ${PROJECT_NAME}
    POST_BUILD
    COMMAND
      ${CMAKE_COMMAND} -E copy_directory $<TARGET_FILE_DIR:engine>/shader
      $<TARGET_FILE_DIR:${PROJECT_NAME}>/shader
    COMMAND
      ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:engine>
      $<TARGET_FILE_DIR:${PROJECT_NAME}>
    COMMAND
      ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:engine>
      $<TARGET_FILE_DIR:${PROJECT_NAME}>
    COMMAND_EXPAND_LISTS
  )
else()
  add_custom_command(
    TARGET ${PROJECT_NAME}
    POST_BUILD
    COMMAND
      ${CMAKE_COMMAND} -E copy_directory $<TARGET_FILE_DIR:engine>/shader
      $<TARGET_FILE_DIR:${PROJECT_NAME}>/shader
    COMMAND
      ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:engine>
      $<TARGET_FILE_DIR:${PROJECT_NAME}>
COMMAND
      ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:glad>
      $<TARGET_FILE_DIR:${PROJECT_NAME}>
  )
endif()
