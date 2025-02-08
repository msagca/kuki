project(Engine)
file(GLOB_RECURSE C_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/source/*.c")
file(GLOB_RECURSE CPP_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp")
add_library(${PROJECT_NAME} SHARED ${C_SOURCES} ${CPP_SOURCES})
set_target_properties(${PROJECT_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
include(GenerateExportHeader)
string(TOUPPER ${PROJECT_NAME} PROJECT_NAME_UPPER)
generate_export_header(
  ${PROJECT_NAME}
  BASE_NAME ${PROJECT_NAME}
  EXPORT_MACRO_NAME ${PROJECT_NAME_UPPER}_API
)
target_include_directories(
  ${PROJECT_NAME}
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries(
  ${PROJECT_NAME}
  PUBLIC
    OpenGL::GL
    assimp
    glad
    glfw
    glm
    imgui
    imgui_filebrowser
    imguizmo
    nlohmann_json
    stb
)
install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/engine_export.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
