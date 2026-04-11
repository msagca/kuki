cmake_minimum_required(VERSION 3.31)
project(kaitai_image)
add_library(${PROJECT_NAME} "${CMAKE_CURRENT_SOURCE_DIR}/src/ico.cpp")
target_include_directories(
  ${PROJECT_NAME}
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(${PROJECT_NAME} PUBLIC kaitai_struct_cpp_stl_runtime)
