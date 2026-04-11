cmake_minimum_required(VERSION 3.31)
project(glad)
find_package(OpenGL REQUIRED)
add_library(${PROJECT_NAME} "${CMAKE_CURRENT_SOURCE_DIR}/src/glad.c")
target_include_directories(
  ${PROJECT_NAME}
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(${PROJECT_NAME} PUBLIC OpenGL::GL)
