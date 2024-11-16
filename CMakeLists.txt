cmake_minimum_required(VERSION 3.20)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(CMAKE_VERSION VERSION_GREATER 3.25)
  set(CMAKE_CXX_STANDARD 23)
else()
  set(CMAKE_CXX_STANDARD 20)
endif()
project("LearningOpenGL")
include(FindDependencies)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
if(CMAKE_GENERATOR MATCHES "Visual Studio")
  foreach(CONFIG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${CONFIG} CONFIG_UPPER)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
  endforeach()
endif()
file(GLOB CPP_SOURCES "${CMAKE_SOURCE_DIR}/src/*.cpp")
file(GLOB GLSL_SOURCES "${CMAKE_SOURCE_DIR}/src/shaders/*.glsl")
add_executable(${PROJECT_NAME} ${CPP_SOURCES} ${GLSL_SOURCES})
target_link_libraries(
  ${PROJECT_NAME}
  PRIVATE glad glfw glm OpenGL::GL stb
)
target_sources(${PROJECT_NAME} PRIVATE ${GLSL_SOURCES})
include_directories(${CMAKE_SOURCE_DIR}/include)
foreach(FILE ${GLSL_SOURCES})
  cmake_path(APPEND FULL_PATH ${CMAKE_CURRENT_SOURCE_DIR} ${FILE})
  list(APPEND SHADER_FILES ${FULL_PATH})
endforeach()
add_custom_command(
  TARGET ${PROJECT_NAME}
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SHADER_FILES} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)
if(WIN32)
  add_custom_command(
    TARGET ${PROJECT_NAME}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${GLFW_DLL} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
  )
endif()
