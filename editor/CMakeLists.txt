project(KukiEditor)
file(GLOB_RECURSE C_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")
file(GLOB_RECURSE CPP_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
set(IMAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/image")
file(GLOB_RECURSE JPG_SOURCES "${IMAGE_SOURCE_DIR}/*.jpg")
file(GLOB_RECURSE PNG_SOURCES "${IMAGE_SOURCE_DIR}/*.png")
set(IMAGE_SOURCES "${JPG_SOURCES}" "${PNG_SOURCES}")
add_executable("${PROJECT_NAME}" "${C_SOURCES}" "${CPP_SOURCES}")
target_include_directories(
  "${PROJECT_NAME}"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(
  "${PROJECT_NAME}"
  PRIVATE kuki_engine imgui imgui_filebrowser imguizmo freetype
)
target_sources("${PROJECT_NAME}" PRIVATE "${IMAGE_SOURCES}")
install(DIRECTORY "${IMAGE_SOURCE_DIR}" DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(TARGETS "${PROJECT_NAME}" DESTINATION "${CMAKE_INSTALL_BINDIR}")
message(
  STATUS
  "Build complete. Binaries will be installed to '${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}'."
)
install(
  CODE
    "message(STATUS \"Installation complete. Binaries are located in '${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}'.\")"
)
if(UNIX AND NOT APPLE)
  set_target_properties(
    "${PROJECT_NAME}"
    PROPERTIES INSTALL_RPATH "$ORIGIN" BUILD_WITH_INSTALL_RPATH TRUE
  )
endif()
add_custom_command(
  TARGET "${PROJECT_NAME}"
  POST_BUILD
  COMMAND
    "${CMAKE_COMMAND}" -E copy_directory "${IMAGE_SOURCE_DIR}"
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/image
  COMMAND
    "${CMAKE_COMMAND}" -E copy_directory $<TARGET_FILE_DIR:kuki_engine>/shader
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/shader
)
