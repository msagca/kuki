project(KukiDuel)
file(
  GLOB_RECURSE CPP_SOURCES
  CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
# The text on screen is drawn with this. Shared with the editor and the other games rather than
# copied here.
set(FONT_SOURCE_DIR "${CMAKE_SOURCE_DIR}/asset/font")
# The one file and not the directory: the editor keeps its icon font there too, which is most of a
# megabyte that a game would carry and never open.
set(FONT_SOURCES "${FONT_SOURCE_DIR}/Inter-VariableFont_opsz,wght.ttf")
# A library the editor can load and an executable that starts it. See `kuki_add_game`.
kuki_add_game(${PROJECT_NAME} ${CPP_SOURCES})
if(KUKI_ENABLE_LINTING AND CLANG_TIDY_EXE)
  enable_clang_tidy(${KUKI_GAME_CODE})
endif()
target_include_directories(
  ${KUKI_GAME_CODE}
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
if(UNIX AND NOT APPLE)
  find_package(X11 REQUIRED)
  target_link_libraries(${PROJECT_NAME} PRIVATE X11::X11)
  set_target_properties(
    ${PROJECT_NAME}
    PROPERTIES INSTALL_RPATH "$ORIGIN" BUILD_WITH_INSTALL_RPATH TRUE
  )
endif()
target_sources(${PROJECT_NAME} PRIVATE ${FONT_SOURCES})
# The fighter, rigged and animated in one .blend and exported to `model/` beside the executable as
# one glTF with its skeleton and every clip. `tools/make_fighter.py` is what wrote the .blend.
kuki_add_blender_models(
  ${PROJECT_NAME}
  "${CMAKE_CURRENT_SOURCE_DIR}/src/model/fighter.blend"
  model
)
# The four stages, one .glb a part, exported to `stage/` beside the executable. `tools/make_stages.py`
# is what wrote the .blend.
kuki_add_blender_models(
  ${PROJECT_NAME}
  "${CMAKE_CURRENT_SOURCE_DIR}/src/model/stages.blend"
  stage
)
install(
  FILES ${FONT_SOURCES}
  DESTINATION "${CMAKE_INSTALL_BINDIR}/font"
  COMPONENT ${PROJECT_NAME}
)
install(
  TARGETS ${PROJECT_NAME}
  DESTINATION "${CMAKE_INSTALL_BINDIR}"
  COMPONENT ${PROJECT_NAME}
)
kuki_hide_console(${PROJECT_NAME})
kuki_report_binary(${PROJECT_NAME})
# Assets resolve against the executable's directory rather than the working directory, so they are
# staged beside it -- see `ResolvePath`.
add_custom_command(
  TARGET ${PROJECT_NAME}
  POST_BUILD
  COMMAND
    ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${PROJECT_NAME}>/font
  COMMAND
    ${CMAKE_COMMAND} -E copy_if_different ${FONT_SOURCES}
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/font
  COMMENT "Staging duel fonts next to the executable"
  VERBATIM
)
if(WIN32 AND DXCOMPILER_DLL AND DXIL_DLL)
  # The Direct3D 12 backend compiles its HLSL at runtime, so the compiler has to be beside the
  # game. Without these it draws nothing.
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
    COMPONENT ${PROJECT_NAME}
  )
endif()
