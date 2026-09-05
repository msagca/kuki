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
file(GLOB_RECURSE COMP_SOURCES CONFIGURE_DEPENDS "${SHADER_SOURCE_DIR}/*.comp")
file(GLOB_RECURSE FRAG_SOURCES CONFIGURE_DEPENDS "${SHADER_SOURCE_DIR}/*.frag")
file(GLOB_RECURSE GEOM_SOURCES CONFIGURE_DEPENDS "${SHADER_SOURCE_DIR}/*.geom")
file(GLOB_RECURSE HLSL_SOURCES CONFIGURE_DEPENDS "${SHADER_SOURCE_DIR}/*.hlsl")
file(GLOB_RECURSE TESC_SOURCES CONFIGURE_DEPENDS "${SHADER_SOURCE_DIR}/*.tesc")
file(GLOB_RECURSE TESE_SOURCES CONFIGURE_DEPENDS "${SHADER_SOURCE_DIR}/*.tese")
file(GLOB_RECURSE VERT_SOURCES CONFIGURE_DEPENDS "${SHADER_SOURCE_DIR}/*.vert")
set(
  SHADER_SOURCES
  ${COMP_SOURCES}
  ${FRAG_SOURCES}
  ${GEOM_SOURCES}
  ${HLSL_SOURCES}
  ${TESC_SOURCES}
  ${TESE_SOURCES}
  ${VERT_SOURCES}
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
# PUBLIC is exactly what a consumer's own code needs, which is now a short list. Everything else
# is an implementation detail this library happens to be built with, and saying so is what lets a
# shared build resolve it internally: private link dependencies of a SHARED library do not
# propagate to consumers at all, and so need not appear in an install(EXPORT) set either. (For a
# STATIC library they still propagate as $<LINK_ONLY:...>, because the consumer's final link needs
# them -- the distinction only pays off once this is a DLL.)
#
# glm is in the API: `Transform`, `Camera` and `Light` are written in its vector and quaternion
# types. glfw is in the API too, less obviously -- `RegisterInputAction` and `IsInputHeld` take
# GLFW key codes, so a game writing `GLFW_KEY_ESCAPE` needs its header. spdlog is here because
# every consumer so far logs with it and expects to keep doing so.
#
# The rest became private when `asset_manager.hpp` stopped declaring Assimp types and defining its
# loaders inline; before that, assimp, nlohmann, stb and tinyexr were all reachable from any header
# that reached the asset manager, which was almost all of them.
target_link_libraries(
  ${PROJECT_NAME}
  PUBLIC OpenGL::GL glfw glm spdlog
  PRIVATE
    assimp
    glad
    kaitai_image
    kaitai_struct_cpp_stl_runtime
    nlohmann_json
    stb
    tinyexr
    whereami
)
if(WIN32)
  # The Direct3D headers are private: `dx_context.hpp` and friends are implementation, and the one
  # backend header a consumer does reach -- `dx_material.hpp`, named by `ComponentVariant` -- needs
  # nothing from the SDK. What does have to be public is the define, because a consumer compiles its
  # own `#ifdef KUKI_HAS_DIRECTX` against the same answer this library was built with; disagreeing
  # about it silently produces a binary with no Direct3D backend and no diagnostic.
  #
  # Stated as a definition rather than by exporting `kuki_d3d12`, whose interface would drag the
  # DirectX-Headers targets into the export set behind it. Anything that genuinely uses the SDK --
  # the editor does -- links `kuki_d3d12` itself.
  target_link_libraries(${PROJECT_NAME} PRIVATE kuki_d3d12)
  target_compile_definitions(${PROJECT_NAME} PUBLIC KUKI_HAS_DIRECTX=1)
endif()
target_sources(${PROJECT_NAME} PRIVATE ${SHADER_SOURCES})
set(EMBEDDED_SHADER_HEADER "${CMAKE_CURRENT_BINARY_DIR}/embedded_shaders.hpp")
add_custom_command(
  OUTPUT "${EMBEDDED_SHADER_HEADER}"
  COMMAND
    ${CMAKE_COMMAND} "-DSHADER_FILES=${SHADER_SOURCES}"
    "-DOUT_FILE=${EMBEDDED_SHADER_HEADER}" -P
    "${CMAKE_SOURCE_DIR}/cmake/EmbedShaders.cmake"
  DEPENDS ${SHADER_SOURCES} "${CMAKE_SOURCE_DIR}/cmake/EmbedShaders.cmake"
  COMMENT "Embedding shader sources into ${EMBEDDED_SHADER_HEADER}"
  VERBATIM
)
target_sources(${PROJECT_NAME} PRIVATE "${EMBEDDED_SHADER_HEADER}")
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
# `model_import.hpp` is deliberately absent: it declares Assimp types and exists only so the asset
# manager's own header need not. Nothing outside this library includes it.
install(
  DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/kuki"
  COMPONENT ${PROJECT_NAME}
  FILES_MATCHING
  PATTERN "*.hpp"
  PATTERN "*.h"
  PATTERN "model_import.hpp" EXCLUDE
)
# Generated, and referenced by 66 of the installed headers, so it ships with them. The embedded
# shader header is generated too but included by no public header, so it stays behind.
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
    "${CMAKE_SOURCE_DIR}/cmake/KukiEngineConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/KukiEngineConfig.cmake"
    INSTALL_DESTINATION "${KUKI_CMAKE_DESTINATION}"
  )
  # SameMajorVersion: the API is C++23 with explicit object parameters and header templates that
  # instantiate against engine internals, so a consumer has to be built by a compatible compiler
  # against a compatible major version. Anything looser would promise more than this can keep.
  write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/KukiEngineConfigVersion.cmake"
    VERSION ${CMAKE_PROJECT_VERSION}
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
