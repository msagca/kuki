# The game. `kuki_add_game` (kuki's `cmake/KukiGame.cmake`) makes the library and the launcher,
# stages the engine's DLLs, the HLSL compiler and the font beside the executable, and installs it.
# Rename the project here, and the target goes with it.
project(KukiTemplate)
file(
  GLOB_RECURSE CPP_SOURCES
  CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
kuki_add_game(${PROJECT_NAME} SOURCES ${CPP_SOURCES} FONT)
