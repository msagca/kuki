project(GettingStarted)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
if(CMAKE_GENERATOR MATCHES "Visual Studio")
  foreach(CONFIG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${CONFIG} CONFIG_UPPER)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG_UPPER} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
  endforeach()
endif()
set(ADDITIONAL_FILES vertShader.glsl fragShader.glsl container.jpg)
add_executable(${PROJECT_NAME} main.cpp ../common/shader.cpp ${ADDITIONAL_FILES})
target_link_libraries(${PROJECT_NAME} PRIVATE glad::glad glfw OpenGL::GL glm::glm)
target_include_directories(${PROJECT_NAME} PRIVATE ${Stb_INCLUDE_DIR})
target_sources(${PROJECT_NAME} PRIVATE ${ADDITIONAL_FILES})
if(NOT "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}" STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}")
  foreach(FILE ${ADDITIONAL_FILES})
    cmake_path(APPEND FULL_PATH ${CMAKE_CURRENT_SOURCE_DIR} ${FILE})
    list(APPEND ADDITIONAL_FILE_PATHS ${FULL_PATH})
  endforeach()
  add_custom_command(
    TARGET ${PROJECT_NAME}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${ADDITIONAL_FILE_PATHS} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
  )
endif()
