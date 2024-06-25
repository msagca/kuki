project(GettingStarted)
add_executable(${PROJECT_NAME} main.cpp shader.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE glad::glad glfw OpenGL::GL)
target_include_directories(${PROJECT_NAME} PRIVATE ${Stb_INCLUDE_DIR})
if(WIN32)
    set(ADDITIONAL_FILES "shader.frag" "shader.vert" "container.jpg")
    add_custom_command(
        TARGET ${PROJECT_NAME}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${ADDITIONAL_FILES} $<CONFIG>
    )
endif()
