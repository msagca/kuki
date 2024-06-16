project(hello-window)
add_executable(${PROJECT_NAME} hello-window.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE glfw OpenGL::GL)
