project(KukiChess)
file(
  GLOB_RECURSE CPP_SOURCES
  CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
set(MATERIAL_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/material")
file(
  GLOB_RECURSE MATERIAL_SOURCES
  CONFIGURE_DEPENDS
  "${MATERIAL_SOURCE_DIR}/*.mat"
)
add_executable(${PROJECT_NAME} ${CPP_SOURCES})
if(KUKI_ENABLE_LINTING AND CLANG_TIDY_EXE)
  enable_clang_tidy(${PROJECT_NAME})
endif()
target_include_directories(
  ${PROJECT_NAME}
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
# The engine and nothing else. No imgui, no imguizmo, no file browser: those are the editor's
# dependencies, and a game linking them would be carrying an editor it never opens.
target_link_libraries(${PROJECT_NAME} PRIVATE KukiEngine)
if(UNIX AND NOT APPLE)
  find_package(X11 REQUIRED)
  target_link_libraries(${PROJECT_NAME} PRIVATE X11::X11)
  set_target_properties(
    ${PROJECT_NAME}
    PROPERTIES INSTALL_RPATH "$ORIGIN" BUILD_WITH_INSTALL_RPATH TRUE
  )
endif()
# Listed as sources so an edited material shows up in the IDE and triggers the staging below.
target_sources(${PROJECT_NAME} PRIVATE ${MATERIAL_SOURCES})
install(
  DIRECTORY "${MATERIAL_SOURCE_DIR}"
  DESTINATION "${CMAKE_INSTALL_BINDIR}"
  COMPONENT KukiGames
)
install(
  TARGETS ${PROJECT_NAME}
  DESTINATION "${CMAKE_INSTALL_BINDIR}"
  COMPONENT KukiGames
)
# Assets resolve against the executable's directory rather than the working directory, so they are
# staged beside it -- see `ResolvePath`. Same shape as the editor's staging, for the same reason.
add_custom_command(
  TARGET ${PROJECT_NAME}
  POST_BUILD
  COMMAND
    ${CMAKE_COMMAND} -E copy_directory "${MATERIAL_SOURCE_DIR}"
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/material
  COMMENT "Staging chess materials next to the executable"
  VERBATIM
)
if(WIN32 AND DXCOMPILER_DLL AND DXIL_DLL)
  # The Direct3D 12 backend compiles its HLSL at runtime, so a game that might select that backend
  # needs the compiler beside it exactly as the editor does. Without these it falls back to OpenGL.
  add_custom_command(
    TARGET ${PROJECT_NAME}
    POST_BUILD
    COMMAND
      ${CMAKE_COMMAND} -E copy_if_different "${DXCOMPILER_DLL}" "${DXIL_DLL}"
      $<TARGET_FILE_DIR:${PROJECT_NAME}>
    COMMENT "Staging the HLSL compiler next to the executable"
    VERBATIM
  )
  install(
    FILES "${DXCOMPILER_DLL}" "${DXIL_DLL}"
    DESTINATION "${CMAKE_INSTALL_BINDIR}"
    COMPONENT KukiGames
  )
endif()
