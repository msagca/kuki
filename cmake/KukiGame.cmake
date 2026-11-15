# What a game's CMakeLists.txt calls, whether the game is in this repository or in one of its own.
#
# Included by the root before anything is added, so every function here exists by the time a game
# is reached. A game outside the repository pins kuki as a submodule and adds it with
# `add_subdirectory`; CMake functions are global once defined, so the same calls work there --
# which is why nothing here may name `CMAKE_SOURCE_DIR` or `CMAKE_BINARY_DIR`. Those are the
# top-level project's, and in a game's repository the top level is the game. Paths into kuki are
# taken from this file's own directory (`CMAKE_CURRENT_FUNCTION_LIST_DIR`) instead, which is kuki's
# `cmake/` wherever kuki was checked out. `template/` is a game laid out the way a repository of its
# own would be, and is built here with the games so it cannot drift from these functions.
#
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
# Copies the shared libraries an executable loads to the directory it is in.
#
# Windows looks for a DLL beside the executable and has no rpath to be told otherwise, so a shared
# build that leaves the engine in `build/engine` produces executables that do not start. Nothing to
# do in a static build, where the list is empty, and the generator expression is why this needs no
# condition of its own.
#
# A target of its own that runs on every build, and not a step after the executable links. That is
# what this was first, and it was wrong in a way that is hard to see: a change to the engine alone
# rebuilds the engine's DLL and relinks nothing that loads it, so the step never ran and the
# executable went on loading the engine from before the change -- built against the new headers,
# run against the old code. `copy_if_different` makes running it every time cost nothing when
# nothing has changed.
function(kuki_stage_runtime target)
  if(NOT WIN32)
    return()
  endif()
  add_custom_target(
    ${target}Runtime
    ALL
    COMMAND
      "${CMAKE_COMMAND}" -E
      "$<IF:$<BOOL:$<TARGET_RUNTIME_DLLS:${target}>>,copy_if_different,true>"
      "$<TARGET_RUNTIME_DLLS:${target}>" "$<TARGET_FILE_DIR:${target}>"
    COMMAND_EXPAND_LISTS
    VERBATIM
  )
  add_dependencies(${target}Runtime ${target})
endfunction()
# Prints where a built executable ended up, so whoever ran the build does not have to go looking.
#
# Called from `kuki_add_game` for a game; anything else calls it itself, in the directory that made
# the target, since `add_custom_command(TARGET ...)` may only be used there.
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
# Puts the HLSL compiler beside an executable, and in its install. The Direct3D 12 backend compiles
# its shaders at run time, so without these nothing is drawn. Found in the Windows SDK by the root.
function(kuki_stage_shader_compiler target component)
  if(NOT (WIN32 AND DXCOMPILER_DLL AND DXIL_DLL))
    return()
  endif()
  add_custom_command(
    TARGET ${target}
    POST_BUILD
    COMMAND
      ${CMAKE_COMMAND} -E copy_if_different "${DXCOMPILER_DLL}" "${DXIL_DLL}"
      $<TARGET_FILE_DIR:${target}>
    COMMENT "Staging the HLSL compiler next to ${target}"
    VERBATIM
  )
  install(
    FILES "${DXCOMPILER_DLL}" "${DXIL_DLL}"
    DESTINATION "${CMAKE_INSTALL_BINDIR}"
    COMPONENT ${component}
  )
endfunction()
# Copies files or directories beside an executable, under `destination`, and installs them there.
#
# Assets resolve against the executable's directory rather than the working directory (see
# `ResolvePath`), so whatever a game opens at run time has to be staged beside it. A target of its
# own rather than a step after linking, for the reason `kuki_stage_runtime` gives: an asset edited
# on its own would otherwise never reach a binary that had not changed. A directory is copied whole;
# a file goes in as itself. Listed on the target too, so the files show up in an IDE.
function(kuki_stage_assets target destination)
  set(files "")
  set(directories "")
  foreach(source ${ARGN})
    if(IS_DIRECTORY "${source}")
      list(APPEND directories "${source}")
      file(GLOB_RECURSE inside CONFIGURE_DEPENDS "${source}/*")
      target_sources(${target} PRIVATE ${inside})
      set_source_files_properties(
        ${inside}
        PROPERTIES HEADER_FILE_ONLY TRUE VS_TOOL_OVERRIDE "None"
      )
    else()
      list(APPEND files "${source}")
      target_sources(${target} PRIVATE "${source}")
      set_source_files_properties(
        "${source}"
        PROPERTIES HEADER_FILE_ONLY TRUE VS_TOOL_OVERRIDE "None"
      )
    endif()
  endforeach()
  set(out "$<TARGET_FILE_DIR:${target}>/${destination}")
  set(
    commands
    COMMAND
    ${CMAKE_COMMAND}
    -E
    make_directory
    "${out}"
  )
  if(files)
    list(
      APPEND commands
      COMMAND
      ${CMAKE_COMMAND}
      -E
      copy_if_different
      ${files}
      "${out}"
    )
    install(
      FILES ${files}
      DESTINATION "${CMAKE_INSTALL_BINDIR}/${destination}"
      COMPONENT ${target}
    )
  endif()
  foreach(directory ${directories})
    list(
      APPEND commands
      COMMAND
      ${CMAKE_COMMAND}
      -E
      copy_directory
      "${directory}"
      "${out}"
    )
    install(
      DIRECTORY "${directory}/"
      DESTINATION "${CMAKE_INSTALL_BINDIR}/${destination}"
      COMPONENT ${target}
    )
  endforeach()
  string(MAKE_C_IDENTIFIER "${destination}" suffix)
  add_custom_target(${target}Assets_${suffix} ALL ${commands} VERBATIM)
  add_dependencies(${target}Assets_${suffix} ${target})
endfunction()
# The font every game here writes its text in, for a game to pass to `kuki_add_game(... FONT)` or
# to `SetOverlayFont` as `font/<this file's name>`. One file and not `asset/font`: the editor keeps
# its icon font there too, which is most of a megabyte a game would carry and never open.
set(
  KUKI_FONT
  "${CMAKE_CURRENT_LIST_DIR}/../asset/font/Inter-VariableFont_opsz,wght.ttf"
)
cmake_path(NORMAL_PATH KUKI_FONT)
set(
  KUKI_FONT
  "${KUKI_FONT}"
  CACHE INTERNAL
  "The font games write their text in"
)
# Declares a game: a library holding everything the game is, and an executable that starts it.
#
#   kuki_add_game(<target>
#     SOURCES <file>...         the game's sources
#     [INCLUDE <dir>...]        its own headers, private to it; `include/` beside the call if absent
#     [ASSETS <dir>...]         directories staged beside the executable under their own names
#     [FONT])                   stages `KUKI_FONT` in `font/`
#
# Everything a game needs to start comes with it: the launcher, the engine's DLLs beside it, the
# HLSL compiler, the console hidden in a release build, its install under a component named after
# it, and the line saying where it landed. A game's own CMakeLists.txt is then this call and
# whatever is particular to it -- `kuki_add_blender_models`, say.
#
# The library is the game. Under BUILD_SHARED_LIBS it is a shared one named `<target>Game`, which
# the game's own executable links -- see `game_module.hpp`. The executable is `game_main.cpp` beside
# this file and nothing else, the same file for every game. A static build has no such library:
# the sources are compiled straight into the executable. Straight in rather than through a static
# library, because the module entry points are found by name and a linker drops an archive member
# nothing refers to.
#
# `KUKI_GAME_CODE` is set for the caller to the target the game's sources are compiled in, which is
# where anything else about its code belongs.
function(kuki_add_game target)
  cmake_parse_arguments(PARSE_ARGV 1 GAME "FONT" "" "SOURCES;INCLUDE;ASSETS")
  if(NOT GAME_SOURCES)
    message(FATAL_ERROR "kuki_add_game(${target}) was given no SOURCES")
  endif()
  if(NOT GAME_INCLUDE AND IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include")
    set(GAME_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/include")
  endif()
  set(entry "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/game_main.cpp")
  if(BUILD_SHARED_LIBS)
    set(code ${target}Game)
    add_library(${code} SHARED ${GAME_SOURCES})
    target_link_libraries(${code} PUBLIC KukiEngine)
    add_executable(${target} "${entry}")
    target_link_libraries(${target} PRIVATE ${code})
    install(
      TARGETS ${code}
      RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT ${target}
      LIBRARY DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT ${target}
    )
  else()
    set(code ${target})
    add_executable(${target} ${GAME_SOURCES} "${entry}")
    target_link_libraries(${target} PRIVATE KukiEngine)
  endif()
  if(GAME_INCLUDE)
    target_include_directories(${code} PRIVATE ${GAME_INCLUDE})
  endif()
  if(KUKI_ENABLE_LINTING AND CLANG_TIDY_EXE)
    enable_clang_tidy(${code})
  endif()
  if(UNIX AND NOT APPLE)
    find_package(X11 REQUIRED)
    target_link_libraries(${target} PRIVATE X11::X11)
    set_target_properties(
      ${target}
      PROPERTIES INSTALL_RPATH "$ORIGIN" BUILD_WITH_INSTALL_RPATH TRUE
    )
  endif()
  install(
    TARGETS ${target}
    DESTINATION "${CMAKE_INSTALL_BINDIR}"
    COMPONENT ${target}
  )
  if(GAME_FONT)
    kuki_stage_assets(${target} font "${KUKI_FONT}")
  endif()
  foreach(directory ${GAME_ASSETS})
    get_filename_component(name "${directory}" NAME)
    kuki_stage_assets(${target} ${name} "${directory}")
  endforeach()
  kuki_stage_runtime(${target})
  kuki_stage_shader_compiler(${target} ${target})
  kuki_hide_console(${target})
  kuki_report_binary(${target})
  # What `games/CMakeLists.txt` lists a game by, for the installer to offer it.
  set_property(TARGET ${target} PROPERTY KUKI_GAME TRUE)
  set(KUKI_GAME_CODE ${code} PARENT_SCOPE)
endfunction()
# Declares a game's tests: gtest cases, compiled with the parts of the game they test.
#
#   kuki_add_game_tests(<target>
#     SOURCES <file>...         the tests, and the game's sources they test
#     [INCLUDE <dir>...])       its headers; `include/` beside the call if absent
#
# Makes `<target>Test`, an executable beside the game's, whose cases ctest finds by running it.
# Nothing when KUKI_BUILD_TESTS is off, so a game calls this unconditionally.
#
# What is tested is a game's rules, which know nothing of a window or a scene -- the duel's
# `duel_sim.cpp`, the frontier's `terrain.cpp` -- so the sources are compiled in as they are, and the
# game's own library is not linked: it holds the systems and the module entry point too, and in a
# static build it is the game's executable. Only `KukiEngine`, as the game itself is given, so a
# test can reach nothing the game could not.
#
# The same call in a repository of its own, whose top-level CMakeLists.txt sets KUKI_BUILD_TESTS
# (kuki then builds googletest, and none of its own tests) and calls `enable_testing()`, without
# which ctest finds nothing.
function(kuki_add_game_tests target)
  if(NOT KUKI_BUILD_TESTS)
    return()
  endif()
  cmake_parse_arguments(PARSE_ARGV 1 TESTS "" "" "SOURCES;INCLUDE")
  if(NOT TESTS_SOURCES)
    message(FATAL_ERROR "kuki_add_game_tests(${target}) was given no SOURCES")
  endif()
  if(NOT TESTS_INCLUDE AND IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include")
    set(TESTS_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/include")
  endif()
  include(GoogleTest)
  set(test ${target}Test)
  add_executable(${test} ${TESTS_SOURCES})
  target_link_libraries(${test} PRIVATE KukiEngine gtest_main)
  if(TESTS_INCLUDE)
    target_include_directories(${test} PRIVATE ${TESTS_INCLUDE})
  endif()
  if(KUKI_ENABLE_LINTING AND CLANG_TIDY_EXE)
    enable_clang_tidy(${test})
  endif()
  kuki_stage_runtime(${test})
  # Discovered when the tests are run and not when the binary links. Discovery runs the binary, and
  # in a shared build it does not start until the engine has been staged beside it, which happens
  # after the link and not as part of it.
  gtest_discover_tests(${test} DISCOVERY_MODE PRE_TEST)
endfunction()
# Blender, for the games whose models are kept as .blend files and exported at build time.
#
# Looked for on PATH first, then where the Windows installer and the macOS bundle put it -- newest
# version first, since a .blend saved by a newer Blender may not open in an older one. Passing
# -DKUKI_BLENDER_EXECUTABLE=<path> outranks all of it.
file(
  GLOB _kuki_blender_dirs
  LIST_DIRECTORIES true
  "$ENV{ProgramFiles}/Blender Foundation/Blender*"
)
list(SORT _kuki_blender_dirs COMPARE NATURAL ORDER DESCENDING)
find_program(
  KUKI_BLENDER_EXECUTABLE
  NAMES blender
  HINTS ${_kuki_blender_dirs} "/Applications/Blender.app/Contents/MacOS"
  DOC "Blender, which exports the games' .blend models to glTF during the build"
)
unset(_kuki_blender_dirs)
# Exports a .blend's `Export` collection to glTF beside a target's executable, one file a mesh.
#
# The .blend is the source and the only copy in the repository; the glTF files are build products,
# as an object file is. Blender runs headless and only when the .blend or the export script has
# changed. See `export_blend_gltf.py` beside this file for what is exported and how files are named.
#
# The export goes to a directory of its own in the build tree and is copied next to the executable
# by the same command, rather than by a POST_BUILD step: that step runs only when the executable
# relinks, and a model edited on its own would never reach a binary that had not changed. The
# install takes the exported directory as it stands.
#
# A game with more than one .blend calls this once for each, with a different destination: each
# export clears its own directory of models before writing, so two sharing one would leave only the
# second's.
#
# Without Blender the target still builds, with a warning and no models, rather than the configure
# failing: a missing model is something a game can report and carry on past, and a build machine
# with no use for the game should not need Blender to build the engine.
function(kuki_add_blender_models target blend destination)
  if(NOT KUKI_BLENDER_EXECUTABLE)
    message(
      WARNING
      "Blender was not found, so ${target} is built without the models in ${blend}. Install Blender or set KUKI_BLENDER_EXECUTABLE."
    )
    return()
  endif()
  set(script "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/export_blend_gltf.py")
  set(exported "${CMAKE_CURRENT_BINARY_DIR}/${destination}.export")
  set(stamp "${CMAKE_CURRENT_BINARY_DIR}/${destination}.stamp")
  add_custom_command(
    OUTPUT "${stamp}"
    COMMAND
      "${KUKI_BLENDER_EXECUTABLE}" --background --factory-startup "${blend}"
      --python-exit-code 1 --python "${script}" -- "${exported}" "${stamp}"
    COMMAND
      ${CMAKE_COMMAND} -E copy_directory "${exported}"
      "$<TARGET_FILE_DIR:${target}>/${destination}"
    DEPENDS "${blend}" "${script}"
    COMMENT "Exporting ${blend} to glTF"
    VERBATIM
  )
  # Named for the destination as well as the target, so a game can export more than one .blend --
  # each to a directory of its own -- without the second call redefining the first one's target.
  string(MAKE_C_IDENTIFIER "${destination}" suffix)
  add_custom_target(${target}Models_${suffix} DEPENDS "${stamp}")
  add_dependencies(${target} ${target}Models_${suffix})
  # Listed on the target so the .blend shows up beside the sources in an IDE.
  target_sources(${target} PRIVATE "${blend}")
  set_source_files_properties(
    "${blend}"
    PROPERTIES HEADER_FILE_ONLY TRUE VS_TOOL_OVERRIDE "None"
  )
  install(
    DIRECTORY "${exported}/"
    DESTINATION "${CMAKE_INSTALL_BINDIR}/${destination}"
    COMPONENT ${target}
  )
endfunction()
