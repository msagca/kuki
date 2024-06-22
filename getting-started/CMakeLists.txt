project(GettingStarted)
add_executable(${PROJECT_NAME} main.cpp shader.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE glad::glad glfw OpenGL::GL)
if(WIN32)
    file(GLOB SHADER_FILES "*.frag" "*.vert")
    add_custom_command(
        TARGET ${PROJECT_NAME}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${SHADER_FILES} $<CONFIG>
    )
endif()
