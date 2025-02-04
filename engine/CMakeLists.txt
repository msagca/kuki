project(Engine)
add_library(${PROJECT_NAME} SHARED)
set_target_properties(${PROJECT_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
include(GenerateExportHeader)
generate_export_header(${PROJECT_NAME})
file(GLOB_RECURSE C_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/source/*.c")
file(GLOB_RECURSE CPP_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp")
target_sources(${PROJECT_NAME} PRIVATE ${C_SOURCES} ${CPP_SOURCES})
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(
  ${PROJECT_NAME}
  PUBLIC OpenGL::GL glad glfw glm stb nlohmann_json assimp
)
install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/engine_export.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
