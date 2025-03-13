project(games)
file(GLOB_RECURSE C_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")
file(GLOB_RECURSE CPP_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
add_executable(${PROJECT_NAME} ${C_SOURCES} ${CPP_SOURCES})
target_include_directories(
  ${PROJECT_NAME}
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(${PROJECT_NAME} PRIVATE engine)
install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES $<TARGET_FILE:engine> DESTINATION ${CMAKE_INSTALL_BINDIR})
if(UNIX AND NOT APPLE)
  set_target_properties(
    ${PROJECT_NAME}
    PROPERTIES INSTALL_RPATH "$ORIGIN" BUILD_WITH_INSTALL_RPATH TRUE
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
    COMMAND
      ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:glfw>
      $<TARGET_FILE_DIR:${PROJECT_NAME}>
  )
endif()
