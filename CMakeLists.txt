cmake_minimum_required(VERSION 3.8)
set(CMAKE_TOOLCHAIN_FILE
    "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
    CACHE STRING
    "vcpkg toolchain file"
)
if(CMAKE_VERSION VERSION_GREATER 3.25)
    set(CMAKE_CXX_STANDARD 23)
elseif(CMAKE_VERSION VERSION_GREATER 3.12)
    set(CMAKE_CXX_STANDARD 20)
endif()
project("LearningOpenGL")
find_package(OpenGL REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
add_subdirectory(getting-started)
