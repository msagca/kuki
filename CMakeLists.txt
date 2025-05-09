cmake_minimum_required(VERSION 3.21)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Export compile commands")
set(CMAKE_CXX_STANDARD 20 CACHE STRING "C++ standard")
set(CMAKE_CXX_STANDARD_REQUIRED ON CACHE BOOL "Require C++ standard")
set(CMAKE_CXX_EXTENSIONS OFF CACHE BOOL "Disable compiler extensions")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Disable shared libraries")
project(Kuki)
option(BUILD_TESTS "Build engine tests" OFF)
option(BUILD_GAMES "Build game samples" OFF)
# Assimp
set(ASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT OFF)
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF)
set(ASSIMP_BUILD_ASSIMP_VIEW OFF)
set(ASSIMP_BUILD_GLTF_IMPORTER ON)
set(ASSIMP_BUILD_TESTS OFF)
set(ASSIMP_INJECT_DEBUG_POSTFIX OFF)
set(ASSIMP_INSTALL_PDB OFF)
set(ASSIMP_WARNINGS_AS_ERRORS OFF)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/external/assimp")
# freetype
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/external/freetype")
# OpenGL
find_package(OpenGL REQUIRED)
# Glad
add_library(glad STATIC "${CMAKE_CURRENT_SOURCE_DIR}/external/glad/src/glad.c")
target_include_directories(
  glad
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/external/glad/include"
)
target_link_libraries(glad PUBLIC OpenGL::GL)
set_target_properties(
  glad
  PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/external/glad"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/external/glad"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/external/glad"
)
# GLFW
set(GLFW_BUILD_DOCS OFF)
set(GLFW_BUILD_EXAMPLES OFF)
set(GLFW_BUILD_TESTS OFF)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/external/glfw")
# GLM
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/external/glm")
# ImGui
set(
  IMGUI_CPP_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/imgui.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/imgui_demo.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/imgui_draw.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/imgui_tables.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/imgui_widgets.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/backends/imgui_impl_glfw.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/backends/imgui_impl_opengl3.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/misc/freetype/imgui_freetype.cpp"
)
add_library(imgui STATIC "${IMGUI_CPP_SOURCES}")
target_include_directories(
  imgui
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/backends"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/misc/freetype"
)
target_link_libraries(imgui PUBLIC OpenGL::GL glfw freetype)
set_target_properties(
  imgui
  PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/external/imgui"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/external/imgui"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/external/imgui"
)
# ImGui file browser
add_library(imgui_filebrowser INTERFACE)
target_include_directories(
  imgui_filebrowser
  INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui-filebrowser"
)
# ImGuizmo
add_library(
  imguizmo
  STATIC
  "${CMAKE_CURRENT_SOURCE_DIR}/external/ImGuizmo/ImGuizmo.cpp"
)
target_include_directories(
  imguizmo
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/external/ImGuizmo"
)
target_link_libraries(imguizmo PUBLIC imgui)
set_target_properties(
  imguizmo
  PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/external/imguizmo"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/external/imguizmo"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/external/imguizmo"
)
# JSON
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/external/json")
# spdlog
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/external/spdlog")
# stb
add_library(stb INTERFACE)
target_include_directories(
  stb
  INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/external/stb"
)
# tinyexr
add_library(
  tinyexr
  STATIC
  "${CMAKE_CURRENT_SOURCE_DIR}/external/tinyexr/deps/miniz/miniz.c"
)
target_include_directories(
  tinyexr
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/external/tinyexr"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/tinyexr/deps/miniz"
)
set_target_properties(
  tinyexr
  PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/external/tinyexr"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/external/tinyexr"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/external/tinyexr"
)
# targets to build
add_subdirectory(engine)
add_subdirectory(editor)
if(BUILD_TESTS)
  add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/external/googletest")
  add_subdirectory(tests)
endif()
if(BUILD_GAMES)
  add_subdirectory(games)
endif()
set_property(
  DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  PROPERTY VS_STARTUP_PROJECT "kuki_editor"
)
