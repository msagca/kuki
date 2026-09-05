cmake_minimum_required(VERSION 3.31)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Export compile commands")
set(CMAKE_CXX_STANDARD 23 CACHE STRING "C++ standard version to use")
set(
  CMAKE_CXX_STANDARD_REQUIRED
  ON
  CACHE BOOL
  "Require the specified C++ standard"
)
set(
  CMAKE_CXX_EXTENSIONS
  OFF
  CACHE BOOL
  "Allow compiler-specific C++ extensions"
)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries")
if(BUILD_SHARED_LIBS)
  set(
    CMAKE_POSITION_INDEPENDENT_CODE
    ON
    CACHE BOOL
    "Enable position independent code"
  )
endif()
# Vendored libraries that have no shared-build support of their own are built static whatever
# BUILD_SHARED_LIBS says, so they end up inside whatever links them instead of becoming a DLL that
# exports nothing. Windows hides every symbol by default, and these have no export annotations:
# glad's API is function-pointer data (which CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS does not cover), and
# the kaitai runtimes, whereami and tinyexr have none at all. Adding them would mean patching a
# submodule, which is not ours to patch -- so they are linked in rather than shipped beside us.
#
# The ones left alone below -- assimp, glfw, glm, spdlog -- carry their own export macros and
# become real DLLs under BUILD_SHARED_LIBS, which is what makes them separately updatable.
macro(kuki_static_subdirectory path)
  set(KUKI_SHARED_BEFORE ${BUILD_SHARED_LIBS})
  set(BUILD_SHARED_LIBS OFF)
  add_subdirectory("${path}")
  set(BUILD_SHARED_LIBS ${KUKI_SHARED_BEFORE})
endmacro()
find_program(CLANG_TIDY_EXE NAMES clang-tidy)
function(enable_clang_tidy target)
  set(
    CLANG_TIDY_CHECKS
    "bugprone-*,performance-*,portability-*,clang-analyzer-*,-bugprone-narrowing-conversions,-bugprone-easily-swappable-parameters"
  )
  set(
    CLANG_TIDY_OPTS
    ${CLANG_TIDY_EXE}
    -checks=${CLANG_TIDY_CHECKS}
    -header-filter=^${CMAKE_CURRENT_SOURCE_DIR}/include/.*
  )
  set_target_properties(
    ${target}
    PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_OPTS}"
  )
endfunction()
project(Kuki VERSION 1.0.0)
string(TOUPPER ${PROJECT_NAME} PROJECT_NAME_UPPER)
include(GNUInstallDirs)
option(${PROJECT_NAME_UPPER}_BUILD_TESTS "Build engine tests" OFF)
option(${PROJECT_NAME_UPPER}_BUILD_GAMES "Build sample games" OFF)
option(${PROJECT_NAME_UPPER}_ENABLE_LINTING "Enable linting" OFF)
# third party libraries
## glm
# First of the vendored projects deliberately. Once install rules are on, assimp, glfw and glm each
# want to add an `uninstall` target; assimp and glfw skip it when one already exists and glm does
# not, so glm has to be the one that gets there first. The alternative is patching a submodule.
set(GLM_BUILD_INSTALL ON CACHE BOOL "")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/external/glm")
## assimp
set(ASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT OFF CACHE BOOL "")
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "")
set(ASSIMP_BUILD_ASSIMP_VIEW OFF CACHE BOOL "")
set(ASSIMP_BUILD_FBX_IMPORTER ON CACHE BOOL "")
set(ASSIMP_BUILD_GLTF_IMPORTER ON CACHE BOOL "")
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "")
set(ASSIMP_INJECT_DEBUG_POSTFIX OFF CACHE BOOL "")
set(ASSIMP_INSTALL ON CACHE BOOL "")
set(ASSIMP_INSTALL_PDB OFF CACHE BOOL "")
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/external/assimp")
## opengl
find_package(OpenGL REQUIRED)
## glad
kuki_static_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/external/glad")
## directx 12 (windows only)
if(WIN32)
  set(DXHEADERS_BUILD_TEST OFF CACHE BOOL "")
  set(DXHEADERS_BUILD_GOOGLE_TEST OFF CACHE BOOL "")
  set(DXHEADERS_INSTALL OFF CACHE BOOL "")
  kuki_static_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/external/DirectX-Headers")
  add_library(kuki_d3d12 INTERFACE)
  target_link_libraries(
    kuki_d3d12
    INTERFACE DirectX-Headers DirectX-Guids d3d12 dxgi dxguid d3dcompiler
  )
  target_compile_definitions(kuki_d3d12 INTERFACE KUKI_HAS_DIRECTX=1)
  get_filename_component(
    WINDOWS_SDK_ROOT
    "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots;KitsRoot10]"
    ABSOLUTE
  )
  file(GLOB WINDOWS_SDK_BIN_DIRS "${WINDOWS_SDK_ROOT}/bin/10.*")
  list(SORT WINDOWS_SDK_BIN_DIRS COMPARE NATURAL ORDER DESCENDING)
  find_file(
    DXCOMPILER_DLL
    NAMES dxcompiler.dll
    PATHS ${WINDOWS_SDK_BIN_DIRS}
    PATH_SUFFIXES x64
    NO_DEFAULT_PATH
    DOC
      "Standalone HLSL compiler, loaded at runtime to build Shader Model 6 bytecode"
  )
  find_file(
    DXIL_DLL
    NAMES dxil.dll
    PATHS ${WINDOWS_SDK_BIN_DIRS}
    PATH_SUFFIXES x64
    NO_DEFAULT_PATH
    DOC
      "Signs compiled bytecode; unsigned bytecode is rejected outside developer mode"
  )
  if(DXCOMPILER_DLL AND DXIL_DLL)
    message(STATUS "HLSL compiler: ${DXCOMPILER_DLL}")
  else()
    message(
      WARNING
      "dxcompiler.dll or dxil.dll not found in the Windows SDK. The Direct3D 12 backend will fail to compile its shaders."
    )
  endif()
endif()
## glfw
set(GLFW_BUILD_DOCS OFF CACHE BOOL "")
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "")
set(GLFW_BUILD_TESTS OFF CACHE BOOL "")
set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "")
set(GLFW_INSTALL ON CACHE BOOL "")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/external/glfw")
## imgui
set(
  IMGUI_CPP_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/imgui.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/imgui_demo.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/imgui_draw.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/imgui_tables.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/imgui_widgets.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/backends/imgui_impl_glfw.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/backends/imgui_impl_opengl3.cpp"
)
if(WIN32)
  list(
    APPEND IMGUI_CPP_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/backends/imgui_impl_dx12.cpp"
  )
endif()
add_library(imgui STATIC ${IMGUI_CPP_SOURCES})
target_include_directories(
  imgui
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/backends"
)
target_link_libraries(imgui PUBLIC OpenGL::GL glfw)
if(WIN32)
  target_link_libraries(imgui PUBLIC kuki_d3d12)
endif()
## imgui-filebrowser
add_library(imgui_filebrowser INTERFACE)
target_include_directories(
  imgui_filebrowser
  INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui-filebrowser"
)
## imguizmo
add_library(
  imguizmo
  STATIC
  "${CMAKE_CURRENT_SOURCE_DIR}/external/ImGuizmo/src/ImGuizmo.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/external/ImGuizmo/src/GraphEditor.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/external/ImGuizmo/src/ImSequencer.cpp"
)
target_include_directories(
  imguizmo
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/external/ImGuizmo/src"
)
target_link_libraries(imguizmo PUBLIC imgui)
## json
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/external/json")
## kaitai-struct (C++ runtime)
set(BUILD_TESTS OFF CACHE BOOL "")
if(WIN32)
  set(STRING_ENCODING_TYPE WIN32API CACHE STRING "")
endif()
kuki_static_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/external/kaitai-struct")
## kaitai-image
kuki_static_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/external/kaitai-image")
## spdlog
set(SPDLOG_INSTALL ON CACHE BOOL "")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/external/spdlog")
## stb
add_library(stb INTERFACE)
target_include_directories(
  stb
  INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/external/stb"
)
## tinyexr
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
## whereami
add_library(
  whereami
  STATIC
  "${CMAKE_CURRENT_SOURCE_DIR}/external/whereami/src/whereami.c"
)
target_include_directories(
  whereami
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/external/whereami/src/"
)
# targets to build
add_subdirectory(engine)
add_subdirectory(editor)
if(${PROJECT_NAME_UPPER}_BUILD_GAMES)
  add_subdirectory(game)
endif()
if(${PROJECT_NAME_UPPER}_BUILD_TESTS)
  enable_testing()
  set(INSTALL_GTEST OFF CACHE BOOL "")
  add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/external/googletest")
  add_subdirectory(test)
endif()
if(WIN32)
  set_property(
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    PROPERTY VS_STARTUP_PROJECT "KukiEditor"
  )
endif()
# installer
set(PROJECT_MAINTAINER "Mustafa Said Agca")
set(PROJECT_URL "https://github.com/msagca/kuki")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A 3D ECS game engine")
set(CPACK_PACKAGE_EXECUTABLES "KukiEditor" "Kuki Editor")
set(CPACK_PACKAGE_HOMEPAGE_URL ${PROJECT_URL})
set(CPACK_PACKAGE_INSTALL_DIRECTORY ${PROJECT_NAME})
set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
set(CPACK_PACKAGE_VENDOR ${PROJECT_MAINTAINER})
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
if(WIN32)
  set(CPACK_NSIS_DISPLAY_NAME ${PROJECT_NAME})
  set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
  set(CPACK_NSIS_URL_INFO_ABOUT ${PROJECT_URL})
elseif(APPLE)
  set(CPACK_BUNDLE_NAME ${PROJECT_NAME})
  set(CPACK_DMG_VOLUME_NAME ${PROJECT_NAME})
elseif(UNIX)
  set(CPACK_DEBIAN_PACKAGE_HOMEPAGE ${PROJECT_URL})
  set(CPACK_DEBIAN_PACKAGE_MAINTAINER ${PROJECT_MAINTAINER})
  set(CPACK_RPM_PACKAGE_LICENSE "MIT")
  set(CPACK_RPM_PACKAGE_URL ${PROJECT_URL})
endif()
include(CPackComponent)
cpack_add_component(
  KukiEngine
  DISPLAY_NAME "Kuki Engine"
  DESCRIPTION "Engine core library."
  HIDDEN
)
cpack_add_component(
  KukiEditor
  DISPLAY_NAME "Kuki Editor"
  DESCRIPTION "An ImGui-based editor application."
  DEPENDS KukiEngine
  REQUIRED
)
set(CPACK_COMPONENTS_ALL KukiEngine KukiEditor)
if(${PROJECT_NAME_UPPER}_BUILD_GAMES)
  cpack_add_component(
    KukiGames
    DISPLAY_NAME "Games"
    DESCRIPTION "Sample games built on the engine."
    DEPENDS KukiEngine
  )
  list(APPEND CPACK_COMPONENTS_ALL KukiGames)
endif()
if(${PROJECT_NAME_UPPER}_BUILD_TESTS)
  cpack_add_component(
    KukiTest
    DISPLAY_NAME "Engine Tests"
    DESCRIPTION "Test binaries to verify engine functionality."
    DEPENDS KukiEngine
    DISABLED
  )
  list(APPEND CPACK_COMPONENTS_ALL KukiTest)
endif()
include(CPack)
