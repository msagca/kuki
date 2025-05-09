project(KukiTests)
file(GLOB_RECURSE C_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")
file(GLOB_RECURSE CPP_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
add_executable("${PROJECT_NAME}" "${C_SOURCES}" "${CPP_SOURCES}")
target_include_directories(
  "${PROJECT_NAME}"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries("${PROJECT_NAME}" PRIVATE kuki_engine gtest)
install(TARGETS "${PROJECT_NAME}" DESTINATION "${CMAKE_INSTALL_BINDIR}")
if(UNIX AND NOT APPLE)
  set_target_properties(
    "${PROJECT_NAME}"
    PROPERTIES INSTALL_RPATH "$ORIGIN" BUILD_WITH_INSTALL_RPATH TRUE
  )
endif()
