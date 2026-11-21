project(KukiEngine)
file(
  GLOB_RECURSE C_SOURCES
  CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)
file(
  GLOB_RECURSE CPP_SOURCES
  CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
set(SHADER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/shader")
# HLSL only. A backend that wants another language adds its glob here; one that can take HLSL
# through dxc (SPIR-V for Vulkan) needs nothing new.
file(
  GLOB_RECURSE SHADER_SOURCES
  CONFIGURE_DEPENDS
  "${SHADER_SOURCE_DIR}/*.hlsl"
)
add_library(${PROJECT_NAME} ${C_SOURCES} ${CPP_SOURCES})
if(KUKI_ENABLE_LINTING AND CLANG_TIDY_EXE)
  enable_clang_tidy(${PROJECT_NAME})
endif()
include(GenerateExportHeader)
generate_export_header(
  ${PROJECT_NAME}
  BASE_NAME kuki_engine
  EXPORT_MACRO_NAME KUKI_ENGINE_API
)
# Split by context because an installed target cannot point at the source tree it was built from.
# Headers are included by bare filename throughout (`#include <asset_manager.hpp>`), so the install
# tree puts them in their own directory and names that directory rather than a parent of it.
target_include_directories(
  ${PROJECT_NAME}
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/kuki>"
)
# `internal/` is the engine's own and is never installed: the backend, the scene loader, the
# shader constants, and `ApplicationInternal`, the half of the application a game is not given. The
# tests are built from this tree and reach it through `KukiEngineInternal`; a game
# reaches `include/` alone, which is all a release ships. Nothing in `include/` may include a header
# from here -- `KukiPublicHeaders` below fails to build if one does.
target_include_directories(
  ${PROJECT_NAME}
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/internal"
)
add_library(KukiEngineInternal INTERFACE)
target_include_directories(
  KukiEngineInternal
  INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/internal"
)
target_link_libraries(KukiEngineInternal INTERFACE ${PROJECT_NAME})
# PUBLIC is exactly what a consumer's own code needs, which is now a short list. Everything else
# is an implementation detail this library happens to be built with, and saying so is what lets a
# shared build resolve it internally: private link dependencies of a SHARED library do not
# propagate to consumers at all, and so need not appear in an install(EXPORT) set either. (For a
# STATIC library they still propagate as $<LINK_ONLY:...>, because the consumer's final link needs
# them -- the distinction only pays off once this is a DLL.)
#
# glm is in the API: `Transform`, `Camera` and `Light` are written in its vector and quaternion
# types. spdlog is here because every consumer so far logs with it and expects to keep doing so.
# glfw is not, though it was: games named their keys by GLFW's constants, so each needed its
# header. They name them by `Key` now (`key.hpp`, GLFW's numbers under the engine's names), and the
# installed headers mention glfw only as the opaque `GLFWwindow` -- the tests, which write triggers
# in GLFW's numbers, link it for themselves.
#
# The rest became private when `asset_manager.hpp` stopped declaring Assimp types and defining its
# loaders inline; before that, assimp, nlohmann, stb and tinyexr were all reachable from any header
# that reached the asset manager, which was almost all of them.
target_link_libraries(
  ${PROJECT_NAME}
  PUBLIC glm spdlog
  PRIVATE
    assimp
    glfw
    kaitai_image
    kaitai_struct_cpp_stl_runtime
    nlohmann_json
    stb
    tinyexr
    whereami
)
if(WIN32)
  # The Direct3D headers are private: `dx_context.hpp` and friends are implementation, in
  # `internal/`, and no installed header reaches one. What does have to be public is the define, because a consumer compiles its
  # own `#ifdef KUKI_HAS_DIRECTX` against the same answer this library was built with; disagreeing
  # about it silently produces a binary with no Direct3D backend and no diagnostic.
  #
  # Stated as a definition rather than by exporting `kuki_d3d12`, whose interface would drag the
  # DirectX-Headers targets into the export set behind it. Anything else that genuinely uses the
  # SDK links `kuki_d3d12` itself.
  target_link_libraries(${PROJECT_NAME} PRIVATE kuki_d3d12)
  target_compile_definitions(${PROJECT_NAME} PUBLIC KUKI_HAS_DIRECTX=1)
  # Winsock, for `UdpTransport`. Private: no header names a socket type.
  target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32)
  # The shell's known folders, for `GetUserDataPath`, and COM's allocator, which frees the path the
  # shell hands back.
  target_link_libraries(${PROJECT_NAME} PRIVATE shell32 ole32)
endif()
# Configuration of the libraries the engine is built on, which has to be seen before their headers
# are, and so is given to the compiler here rather than written at the top of each file that needs
# it. It was written at the top of each -- `GLM_ENABLE_EXPERIMENTAL` in eight files,
# `GLFW_INCLUDE_NONE` in three, the two Windows ones in three -- and each was right only for as long
# as nothing included the library's header ahead of it. The engine's own macros are in
# `macros.hpp`, which says what is kept where.
#
# Public where a consumer meets the same headers through the engine's: glm's extensions, and the
# Windows headers, whose `min` and `max` macros would otherwise break `std::min` in a game. Private
# where it is how the engine uses GLFW, which a game includes only for the names of its keys.
target_compile_definitions(
  ${PROJECT_NAME}
  PUBLIC GLM_ENABLE_EXPERIMENTAL
  PRIVATE GLFW_INCLUDE_NONE
)
if(WIN32)
  target_compile_definitions(
    ${PROJECT_NAME}
    PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN
    PRIVATE GLFW_EXPOSE_NATIVE_WIN32
  )
endif()
# Listed so they show up in an IDE, and marked so nothing tries to compile them as sources of the
# engine. Every one of these is compiled to DXIL by `KukiShaderCompiler` below instead.
#
# The marking is not cosmetic. MSBuild assigns `.hlsl` to FxCompile on sight, so the Visual Studio
# generator was calling FXC on shaders that have no `main` -- every entry point here is named for
# its stage (`VSMain`, `PSMain`, `CSTrace`) -- and failing the build with X3501 before a single
# translation unit was compiled. FXC would be the wrong compiler even if it found one: these are
# Shader Model 6, which only dxc emits.
target_sources(${PROJECT_NAME} PRIVATE ${SHADER_SOURCES})
set_source_files_properties(
  ${SHADER_SOURCES}
  PROPERTIES HEADER_FILE_ONLY TRUE VS_TOOL_OVERRIDE "None"
)
# The shaders are compiled while the engine is built, by a tool built first for the purpose, and
# their DXIL is compiled into the engine as a generated source (`FindCompiledShader`). They were
# embedded as text and compiled by `dxcompiler.dll` at run time, the first time each pipeline was
# asked for, which was most of the seconds every game's first frame took; and a shader that did not
# compile showed only when run. Now a shader that does not compile fails the build.
#
# A tool and not `dxc.exe` run once per entry point, because the shaders are given
# `SHADER_DEFINITIONS`, values the C++ decides: the tool includes `shader_definitions.hpp` and so
# hands them over as they are, where a command line would need them copied out of the C++ by some
# other step first. It is rebuilt when they change, and the shaders with it, since the command
# depends on the tool.
#
# The compiler is copied beside the tool, which loads it from there: `dxcompiler.dll` loads
# `dxil.dll`, which signs the output, by bare name, and so finds it only in the directory of the
# executable. Nothing else is given either, since nothing else compiles HLSL any more.
if(WIN32)
  add_executable(
    KukiShaderCompiler
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/compile_shaders.cpp"
  )
  target_include_directories(
    KukiShaderCompiler
    PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/include"
      "${CMAKE_CURRENT_SOURCE_DIR}/internal"
      "${CMAKE_CURRENT_BINARY_DIR}"
  )
  target_link_libraries(KukiShaderCompiler PRIVATE glm)
  # Not WIN32_LEAN_AND_MEAN, as the engine is: it leaves out the COM headers `dxcapi.h` is
  # written against (`IUnknown`, `IStream`).
  target_compile_definitions(
    KukiShaderCompiler
    PRIVATE GLM_ENABLE_EXPERIMENTAL NOMINMAX
  )
  add_custom_command(
    TARGET KukiShaderCompiler
    POST_BUILD
    COMMAND
      ${CMAKE_COMMAND} -E copy_if_different "${DXCOMPILER_DLL}" "${DXIL_DLL}"
      $<TARGET_FILE_DIR:KukiShaderCompiler>
    COMMENT "Staging the HLSL compiler next to KukiShaderCompiler"
    VERBATIM
  )
  # One file a configuration, since Debug and Release compile the shaders differently (Debug
  # without optimisation, its debug information in PDBs beside the file for PIX).
  set(
    COMPILED_SHADER_SOURCE
    "${CMAKE_CURRENT_BINARY_DIR}/compiled_shaders_$<CONFIG>.cpp"
  )
  add_custom_command(
    OUTPUT "${COMPILED_SHADER_SOURCE}"
    COMMAND
      KukiShaderCompiler --output "${COMPILED_SHADER_SOURCE}"
      "$<$<CONFIG:Debug>:--debug;--pdb;${CMAKE_CURRENT_BINARY_DIR}/shader_pdb_$<CONFIG>>"
      ${SHADER_SOURCES}
    DEPENDS KukiShaderCompiler ${SHADER_SOURCES}
    COMMENT "Compiling the shaders to DXIL"
    COMMAND_EXPAND_LISTS
    VERBATIM
  )
  target_sources(${PROJECT_NAME} PRIVATE "${COMPILED_SHADER_SOURCE}")
  # Bytes, and a lookup over them: nothing for clang-tidy to find, and a long file for it to read.
  set_source_files_properties(
    "${COMPILED_SHADER_SOURCE}"
    PROPERTIES SKIP_LINTING TRUE
  )
endif()
# A find_package-able install is generated for a shared build only, and the reason is CMake's,
# not a shortcut. install(EXPORT) requires every target the exported one links to be exported as
# well; a SHARED library links its private dependencies internally and so declares none of them,
# leaving glm, glfw and spdlog, which install themselves. A STATIC library declares all twelve as
# $<LINK_ONLY:...>, because the consumer's own final link needs every one -- and half of those are
# vendored subprojects with no install rules of their own and include paths pointing into this
# source tree, so exporting them would mean carrying patches for six submodules.
#
# So: a static build installs the archive as it always did, and a shared build installs a package.
# That is also the arrangement worth having -- see the note on target_link_libraries above.
if(BUILD_SHARED_LIBS)
  if(WIN32)
    # The libraries the engine's own DLL loads -- assimp, glfw, spdlog -- under the engine's
    # component. They have install rules of their own, but under components of their own naming
    # that the installer is not told to include, so a package built from those rules alone holds an
    # engine that cannot be loaded. Windows wants them beside it and has no other place to look.
    install(
      FILES $<TARGET_RUNTIME_DLLS:${PROJECT_NAME}>
      DESTINATION "${CMAKE_INSTALL_BINDIR}"
      COMPONENT ${PROJECT_NAME}
    )
  endif()
  install(
    TARGETS ${PROJECT_NAME}
    EXPORT KukiEngineTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_BINDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT ${PROJECT_NAME}
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/kuki"
  )
else()
  install(
    TARGETS ${PROJECT_NAME}
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_BINDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT ${PROJECT_NAME}
  )
  message(
    STATUS
    "KukiEngine: static build, so no CMake package is generated. Configure with -DBUILD_SHARED_LIBS=ON for a find_package(KukiEngine) install."
  )
endif()
# What tells an installed executable it is installed, so it keeps its settings and caches in the
# user's data folder rather than beside itself (`GetUserDataPath`). Installed and never staged: the
# build tree has no such file, so a development build writes beside itself as it always has. Under
# the engine's component, which every game's depends on, and from this file rather than the root's,
# so a game in a repository of its own, which adds only the engine, installs it too.
install(
  FILES "${Kuki_SOURCE_DIR}/cmake/kuki.installed"
  DESTINATION "${CMAKE_INSTALL_BINDIR}"
  COMPONENT ${PROJECT_NAME}
)
# `include/` whole and nothing else. What a game is not to reach is in `internal/`, so there is no
# list of exclusions to keep in step -- `model_import.hpp`, which declares Assimp types, was one.
install(
  DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/kuki"
  COMPONENT ${PROJECT_NAME}
  FILES_MATCHING
  PATTERN "*.hpp"
  PATTERN "*.h"
)
# Generated, and referenced by 66 of the installed headers, so it ships with them. The compiled
# shaders are generated too, but as a source compiled into the engine, so nothing of them ships.
install(
  FILES "${CMAKE_CURRENT_BINARY_DIR}/kuki_engine_export.h"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/kuki"
  COMPONENT ${PROJECT_NAME}
)
if(BUILD_SHARED_LIBS)
  include(CMakePackageConfigHelpers)
  set(KUKI_CMAKE_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/KukiEngine")
  install(
    EXPORT KukiEngineTargets
    FILE KukiEngineTargets.cmake
    NAMESPACE Kuki::
    DESTINATION "${KUKI_CMAKE_DESTINATION}"
    COMPONENT ${PROJECT_NAME}
  )
  configure_package_config_file(
    "${Kuki_SOURCE_DIR}/cmake/KukiEngineConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/KukiEngineConfig.cmake"
    INSTALL_DESTINATION "${KUKI_CMAKE_DESTINATION}"
  )
  # SameMajorVersion: the API is C++23 with explicit object parameters and header templates that
  # instantiate against engine internals, so a consumer has to be built by a compatible compiler
  # against a compatible major version. Anything looser would promise more than this can keep.
  write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/KukiEngineConfigVersion.cmake"
    VERSION ${Kuki_VERSION}
    COMPATIBILITY SameMajorVersion
  )
  install(
    FILES
      "${CMAKE_CURRENT_BINARY_DIR}/KukiEngineConfig.cmake"
      "${CMAKE_CURRENT_BINARY_DIR}/KukiEngineConfigVersion.cmake"
    DESTINATION "${KUKI_CMAKE_DESTINATION}"
    COMPONENT ${PROJECT_NAME}
  )
endif()
