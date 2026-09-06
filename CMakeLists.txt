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
# Hides the console window a release build of an application would otherwise open behind it.
#
# Windows decides this from the executable's subsystem, which is a property of the binary rather
# than anything it can choose at run time: a CONSOLE image is given a console by the loader before
# any of its code runs, so there is no point at which the program could decline one. Hence a link
# flag rather than a call to `FreeConsole`, which would flash a window and then close it.
#
# Debug keeps its console, which is where the logs go -- see `Application`, which turns logging off
# in release builds for the same reason this hides the window: neither is anything to show someone
# playing a game.
#
# The entry point has to be named explicitly. `/SUBSYSTEM:WINDOWS` makes the CRT look for `WinMain`,
# and every application here has an ordinary `main`; pointing the linker back at `mainCRTStartup`
# keeps that working rather than making each one carry a second entry point for release builds.
function(kuki_hide_console target)
  if(NOT WIN32)
    return()
  endif()
  set_target_properties(
    ${target}
    PROPERTIES WIN32_EXECUTABLE "$<NOT:$<CONFIG:Debug>>"
  )
  target_link_options(
    ${target}
    PRIVATE "$<$<NOT:$<CONFIG:Debug>>:LINKER:/ENTRY:mainCRTStartup>"
  )
endfunction()
# Prints where a built executable ended up, so whoever ran the build does not have to go looking.
#
# A function each target calls rather than something applied centrally, because
# `add_custom_command(TARGET ...)` may only be used in the directory that created the target --
# so `game/CMakeLists.txt`, which discovers games without reading their files, cannot add it on
# their behalf however much it would like to.
function(kuki_report_binary target)
  add_custom_command(
    TARGET ${target}
    POST_BUILD
    COMMAND
      ${CMAKE_COMMAND} -E cmake_echo_color
      "Build successful. Binaries can be found at: $<TARGET_FILE:${target}>"
    VERBATIM
  )
endfunction()
project(Kuki VERSION 1.0.0)
string(TOUPPER ${PROJECT_NAME} PROJECT_NAME_UPPER)
# Whether a directory can actually be written to, asked by writing to it.
#
# A real creation rather than an inspection of permissions, because that is the question that
# matters and it is the one Windows answers least predictably from the outside: an inherited deny,
# a read-only attribute and a missing elevation all end differently in the ACL and identically at
# the point of writing.
#
# The prefix itself usually does not exist yet, so what is probed is the nearest ancestor that
# does -- which is the directory the install would have to create the prefix inside.
function(kuki_directory_is_writable directory result)
  set(existing "${directory}")
  while(NOT EXISTS "${existing}")
    get_filename_component(parent "${existing}" DIRECTORY)
    if(parent STREQUAL "" OR parent STREQUAL existing)
      break()
    endif()
    set(existing "${parent}")
  endwhile()
  set(probe "${existing}/.kuki-install-probe")
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E make_directory "${probe}"
    RESULT_VARIABLE status
    OUTPUT_QUIET
    ERROR_QUIET
  )
  if(status EQUAL 0)
    execute_process(
      COMMAND ${CMAKE_COMMAND} -E rm -rf "${probe}"
      OUTPUT_QUIET
      ERROR_QUIET
    )
    set(${result} TRUE PARENT_SCOPE)
  else()
    set(${result} FALSE PARENT_SCOPE)
  endif()
endfunction()
# Where `cmake --install` puts things when nobody has said otherwise, which on Windows is under
# Program Files and needs an elevated shell to write to. A build configured from an ordinary
# terminal would otherwise get all the way to the install before failing on the first copy, so the
# prefix moves into the user's own tree instead -- the arrangement `pip install --user` and
# `cargo install` both settle on, and the reason `~/.local` exists.
#
# Only when the prefix is CMake's own default. Anything given with -DCMAKE_INSTALL_PREFIX, set by a
# preset, or already in the cache from an earlier configure is a decision somebody made, and is left
# alone even if it turns out to be unwritable: being quietly redirected somewhere other than where
# you said is worse than being told you cannot write where you said.
#
# Decided here, at configure time, because that is when the prefix is baked into the install script.
# A build configured unelevated and installed from an elevated shell afterwards still installs to
# the fallback -- reconfigure with -DCMAKE_INSTALL_PREFIX to say otherwise.
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  kuki_directory_is_writable("${CMAKE_INSTALL_PREFIX}" KUKI_PREFIX_WRITABLE)
  if(NOT KUKI_PREFIX_WRITABLE)
    if(WIN32)
      file(TO_CMAKE_PATH "$ENV{USERPROFILE}" KUKI_HOME)
    else()
      file(TO_CMAKE_PATH "$ENV{HOME}" KUKI_HOME)
    endif()
    if(KUKI_HOME AND IS_DIRECTORY "${KUKI_HOME}")
      set(KUKI_FALLBACK_PREFIX "${KUKI_HOME}/.local")
      message(
        STATUS
        "Install prefix: ${CMAKE_INSTALL_PREFIX} is not writable, so ${KUKI_FALLBACK_PREFIX} is used instead. Run the configure from an elevated shell, or pass -DCMAKE_INSTALL_PREFIX, to install elsewhere."
      )
      set(
        CMAKE_INSTALL_PREFIX
        "${KUKI_FALLBACK_PREFIX}"
        CACHE PATH
        "Install path prefix, prepended onto install directories"
        FORCE
      )
    else()
      # Nothing to fall back to, so the default stands and the install fails where it always would.
      # Said now rather than then, because a warning at configure time is one somebody reads.
      message(
        WARNING
        "Install prefix: ${CMAKE_INSTALL_PREFIX} is not writable and no home directory could be found to fall back on. `cmake --install` will fail unless it is run elevated or given -DCMAKE_INSTALL_PREFIX."
      )
    endif()
  endif()
endif()
include(GNUInstallDirs)
option(${PROJECT_NAME_UPPER}_BUILD_TESTS "Build engine tests" OFF)
option(${PROJECT_NAME_UPPER}_BUILD_GAMES "Build sample games" OFF)
option(${PROJECT_NAME_UPPER}_ENABLE_LINTING "Enable linting" OFF)
# Claims the cache entry for the generator list before any vendored project can.
#
# CPack picks its generators from the platform when this is empty -- an installer on Windows, a
# tarball on Linux, a drag-and-drop bundle on macOS -- and picking them per platform is exactly
# what this project wants, so empty is the value it wants to keep.
#
# It has to be claimed rather than simply left alone, because `external/spdlog/cmake/spdlogCPack.cmake`
# pins it to "TGZ;ZIP" for spdlog's own releases and does it in the cache, which every project in
# the build shares. That file is reached because SPDLOG_INSTALL is on, which is wanted for the
# install rules that come with it, so the setting arrives as a side effect of something worth
# having. The effect was that a vendored logging library decided what this project ships: two
# archives, on a machine with NSIS installed and an installer configured for it.
#
# Writing the entry first is what settles it. `set(... CACHE ...)` without FORCE only writes when
# the entry does not already exist, so spdlog's copy becomes a no-op -- and by the same rule this
# one is a no-op when someone has passed -DCPACK_GENERATOR themselves, which is the one answer that
# should outrank both.
set(
  CPACK_GENERATOR
  ""
  CACHE STRING
  "Generators CPack builds packages with. Empty means the platform's own default."
)
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
# The Start Menu shortcut is written by hand rather than through CPACK_PACKAGE_EXECUTABLES, which
# would place it in the installer's core section -- the part that runs whatever was selected. With
# the editor now optional, that would leave anyone who unticked it holding a Start Menu entry that
# points at an executable which was never installed.
#
# Guarded by the section's own flag instead, in plain NSIS: `SectionGetFlags` and a bit test against
# 1, which is what `SF_SELECTED` is. LogicLib's `${If}` would read better, but CPack's template
# includes only WinMessages.NSH, and adding includes to reach it is a larger change than the two
# lines it would save.
#
# Escaped for one more round than looks necessary, and written as a bracket argument so that what is
# escaped is exactly what appears here. This text is not handed to NSIS directly: CPack copies it
# verbatim into `CPackConfig.cmake` as a quoted string and parses that file as CMake again on the
# way to generating the installer script. So the quotes, the backslashes NSIS wants in its paths,
# and `${KukiEditor}` -- which is NSIS's reference to the section, not a CMake variable -- all have
# to survive that second parse, and each is escaped for it here.
set(
  CPACK_NSIS_CREATE_ICONS_EXTRA
  [[
  SectionGetFlags \${KukiEditor} $R9
  IntOp $R9 $R9 & 1
  StrCmp $R9 1 0 +2
    CreateShortCut \"$SMPROGRAMS\\$STARTMENU_FOLDER\\Kuki Editor.lnk\" \"$INSTDIR\\bin\\KukiEditor.exe\"
  ]]
)
# Deleted unconditionally, since removing a shortcut that was never created is not an error and the
# uninstaller has no record of what was ticked when it was installed.
set(
  CPACK_NSIS_DELETE_ICONS_EXTRA
  [[
  Delete \"$SMPROGRAMS\\$MUI_TEMP\\Kuki Editor.lnk\"
  ]]
)
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
# Not REQUIRED, so someone who only wants to play the games can leave it out. Selected by default
# all the same: it is what this project mostly is.
cpack_add_component(
  KukiEditor
  DISPLAY_NAME "Kuki Editor"
  DESCRIPTION "An ImGui-based editor application."
  DEPENDS KukiEngine
)
set(CPACK_COMPONENTS_ALL KukiEngine KukiEditor)
# One entry per game rather than one for all of them, gathered under a group so they read as a set
# in the installer's tree. The names come from `game/CMakeLists.txt`, which discovers them, so a new
# game appears here by existing rather than by being listed.
if(${PROJECT_NAME_UPPER}_BUILD_GAMES AND KUKI_GAME_TARGETS)
  cpack_add_component_group(
    Games
    DISPLAY_NAME "Games"
    DESCRIPTION "Sample games built on the engine."
  )
  foreach(GAME_TARGET ${KUKI_GAME_TARGETS})
    # `KukiChess` reads as `Kuki Chess` in the tree, matching the two components above.
    string(REGEX REPLACE "^${PROJECT_NAME}" "${PROJECT_NAME} " GAME_DISPLAY_NAME "${GAME_TARGET}")
    cpack_add_component(
      ${GAME_TARGET}
      DISPLAY_NAME "${GAME_DISPLAY_NAME}"
      DESCRIPTION "The ${GAME_DISPLAY_NAME} game and the assets it runs on."
      GROUP Games
      DEPENDS KukiEngine
    )
    list(APPEND CPACK_COMPONENTS_ALL ${GAME_TARGET})
  endforeach()
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
