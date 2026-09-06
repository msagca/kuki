project(KukiEditor)
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
set(IMAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/image")
file(GLOB_RECURSE JPG_SOURCES CONFIGURE_DEPENDS "${IMAGE_SOURCE_DIR}/*.jpg")
file(GLOB_RECURSE PNG_SOURCES CONFIGURE_DEPENDS "${IMAGE_SOURCE_DIR}/*.png")
set(IMAGE_SOURCES ${JPG_SOURCES} ${PNG_SOURCES})
# Shared with the games rather than owned here: a font is the one asset more than one target in
# this repository wants, and the file is most of a megabyte. See `asset/font`.
set(FONT_SOURCE_DIR "${CMAKE_SOURCE_DIR}/asset/font")
file(GLOB_RECURSE TTF_SOURCES CONFIGURE_DEPENDS "${FONT_SOURCE_DIR}/*.ttf")
set(FONT_SOURCES ${TTF_SOURCES})
set(MATERIAL_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/material")
set(SCENE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/scene")
file(GLOB_RECURSE SCENE_SOURCES CONFIGURE_DEPENDS "${SCENE_SOURCE_DIR}/*.json")
add_executable(${PROJECT_NAME} ${C_SOURCES} ${CPP_SOURCES})
if(KUKI_ENABLE_LINTING AND CLANG_TIDY_EXE)
  enable_clang_tidy(${PROJECT_NAME})
endif()
target_include_directories(
  ${PROJECT_NAME}
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(
  ${PROJECT_NAME}
  PRIVATE KukiEngine imgui imgui_filebrowser imguizmo
)
if(WIN32)
  # Declared here rather than inherited from the engine: this target includes `dx_context.hpp` and
  # calls into the swap chain itself, so the Direct3D headers are its own dependency.
  target_link_libraries(${PROJECT_NAME} PRIVATE kuki_d3d12)
endif()
if(UNIX AND NOT APPLE)
  find_package(X11 REQUIRED)
  target_link_libraries(${PROJECT_NAME} PRIVATE X11::X11)
endif()
target_sources(
  ${PROJECT_NAME}
  PRIVATE ${FONT_SOURCES} ${IMAGE_SOURCES} ${SCENE_SOURCES}
)
target_sources(${PROJECT_NAME} PRIVATE "${IMAGE_SOURCE_DIR}/kuki.rc")
install(
  DIRECTORY "${IMAGE_SOURCE_DIR}"
  DESTINATION "${CMAKE_INSTALL_BINDIR}"
  COMPONENT ${PROJECT_NAME}
)
install(
  DIRECTORY "${FONT_SOURCE_DIR}"
  DESTINATION "${CMAKE_INSTALL_BINDIR}"
  COMPONENT ${PROJECT_NAME}
)
install(
  DIRECTORY "${SCENE_SOURCE_DIR}"
  DESTINATION "${CMAKE_INSTALL_BINDIR}"
  COMPONENT ${PROJECT_NAME}
)
install(
  DIRECTORY "${MATERIAL_SOURCE_DIR}"
  DESTINATION "${CMAKE_INSTALL_BINDIR}"
  COMPONENT ${PROJECT_NAME}
)
install(
  TARGETS ${PROJECT_NAME}
  DESTINATION "${CMAKE_INSTALL_BINDIR}"
  COMPONENT ${PROJECT_NAME}
)
install(
  CODE
    "message(STATUS \"Installation complete. Binaries can be found at: ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}\")"
)
if(UNIX AND NOT APPLE)
  set_target_properties(
    ${PROJECT_NAME}
    PROPERTIES INSTALL_RPATH "$ORIGIN" BUILD_WITH_INSTALL_RPATH TRUE
  )
endif()
set(SCENE_STAMP "${CMAKE_CURRENT_BINARY_DIR}/scene_assets.stamp")
add_custom_command(
  OUTPUT "${SCENE_STAMP}"
  COMMAND
    ${CMAKE_COMMAND} -E copy_directory "${SCENE_SOURCE_DIR}"
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/scene
  COMMAND ${CMAKE_COMMAND} -E touch "${SCENE_STAMP}"
  DEPENDS ${SCENE_SOURCES}
  COMMENT "Copying editor scene files"
  VERBATIM
)
add_custom_target(${PROJECT_NAME}Scenes DEPENDS "${SCENE_STAMP}")
add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}Scenes)
if(WIN32 AND DXCOMPILER_DLL AND DXIL_DLL)
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
add_custom_command(
  TARGET ${PROJECT_NAME}
  POST_BUILD
  COMMAND
    ${CMAKE_COMMAND} -E copy_directory "${FONT_SOURCE_DIR}"
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/font
  COMMAND
    ${CMAKE_COMMAND} -E copy_directory "${IMAGE_SOURCE_DIR}"
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/image
  COMMAND
    ${CMAKE_COMMAND} -E copy_directory "${MATERIAL_SOURCE_DIR}"
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/material
)
kuki_hide_console(${PROJECT_NAME})
kuki_report_binary(${PROJECT_NAME})
