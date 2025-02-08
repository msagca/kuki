cmake_minimum_required(VERSION 3.21)
project(LearningOpenGL)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
# GLFW
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/external/glfw)
# GLM
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/external/glm)
# JSON
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/external/json)
# Assimp
set(ASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT OFF)
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF)
set(ASSIMP_BUILD_ASSIMP_VIEW OFF)
set(ASSIMP_BUILD_GLTF_IMPORTER ON)
set(ASSIMP_BUILD_TESTS OFF)
set(ASSIMP_INJECT_DEBUG_POSTFIX OFF)
set(ASSIMP_INSTALL OFF)
set(ASSIMP_INSTALL_PDB OFF)
set(ASSIMP_WARNINGS_AS_ERRORS OFF)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/external/assimp)
# OpenGL
find_package(OpenGL REQUIRED)
# ImGui
set(
  IMGUI_CPP_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/imgui.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/imgui_demo.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/imgui_draw.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/imgui_tables.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/imgui_widgets.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/backends/imgui_impl_glfw.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/backends/imgui_impl_opengl3.cpp
)
add_library(imgui STATIC ${IMGUI_CPP_SOURCES})
target_include_directories(
  imgui
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/backends
)
target_link_libraries(imgui PUBLIC OpenGL::GL glfw)
set_target_properties(
  imgui
  PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/external/imgui
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/external/imgui
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/external/imgui
)
# ImGuizmo
set(
  IMGUIZMO_CPP_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/external/ImGuizmo/ImGuizmo.cpp
)
add_library(imguizmo STATIC ${IMGUIZMO_CPP_SOURCES})
target_include_directories(
  imguizmo
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/external/ImGuizmo
)
target_link_libraries(imguizmo PUBLIC imgui)
set_target_properties(
  imguizmo
  PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/external/imguizmo
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/external/imguizmo
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/external/imguizmo
)
# Glad
add_library(glad SHARED ${CMAKE_CURRENT_SOURCE_DIR}/external/glad/src/glad.c)
target_include_directories(
  glad
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/external/glad/include
)
target_link_libraries(glad PUBLIC OpenGL::GL)
target_compile_definitions(
  glad
  PRIVATE GLAD_GLAPI_EXPORT_BUILD
  PUBLIC GLAD_GLAPI_EXPORT
)
set_target_properties(
  glad
  PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/external/glad
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/external/glad
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/external/glad
)
# stb
add_library(stb INTERFACE)
target_include_directories(
  stb
  INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/external/stb
)
# ImGui file browser
add_library(imgui_filebrowser INTERFACE)
target_include_directories(
  imgui_filebrowser
  INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui-filebrowser
)
# engine, editor, game
add_subdirectory(engine)
add_subdirectory(editor)
# add_subdirectory(game)
set_property(
  DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  PROPERTY VS_STARTUP_PROJECT "Editor"
)
