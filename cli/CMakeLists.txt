# `kuki`, the command a game's repository is started and looked after with. See `src/main.cpp`.
#
# Plain C++ and the standard library: it links nothing of the engine, so it carries none of its
# DLLs and runs before any engine is built. The C runtime is linked in statically for the same
# reason -- installed on the PATH, it should start on a machine with no Visual C++ redistributable.
project(KukiCli)
add_executable(${PROJECT_NAME} src/main.cpp src/game_name.cpp src/process.cpp)
set_target_properties(
  ${PROJECT_NAME}
  PROPERTIES
    OUTPUT_NAME kuki
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)
target_include_directories(
  ${PROJECT_NAME}
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_compile_definitions(
  ${PROJECT_NAME}
  PRIVATE KUKI_VERSION="${Kuki_VERSION}"
)
if(KUKI_ENABLE_LINTING AND CLANG_TIDY_EXE)
  enable_clang_tidy(${PROJECT_NAME})
endif()
# Beside the engine in `bin`, the directory the Windows installer offers to put on the PATH
# (`cmake/installer_path.iss`).
install(
  TARGETS ${PROJECT_NAME}
  DESTINATION "${CMAKE_INSTALL_BINDIR}"
  COMPONENT ${PROJECT_NAME}
)
kuki_report_binary(${PROJECT_NAME})
