project(test)
file(GLOB_RECURSE C_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/source/*.c")
file(GLOB_RECURSE CPP_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp")
add_executable(${PROJECT_NAME} ${C_SOURCES} ${CPP_SOURCES})
target_include_directories(
  ${PROJECT_NAME}
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(${PROJECT_NAME} PRIVATE engine gtest)
add_custom_command(
  TARGET ${PROJECT_NAME}
  POST_BUILD
  COMMAND
    ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:engine>
    $<TARGET_FILE_DIR:${PROJECT_NAME}>
  COMMAND
    ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:engine>
    $<TARGET_FILE_DIR:${PROJECT_NAME}>
  COMMAND_EXPAND_LISTS
)
