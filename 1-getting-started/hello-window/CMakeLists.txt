project(hello-window)
add_executable(${PROJECT_NAME} ${PROJECT_NAME}.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE glad::glad glfw OpenGL::GL)
