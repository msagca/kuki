# The game. `kuki_add_game` (the engine's `cmake/KukiGame.cmake`) makes the library and the launcher,
# stages the engine's DLLs, the HLSL compiler and the font beside the executable, and installs it.
file(
  GLOB_RECURSE CPP_SOURCES
  CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
kuki_add_game(MyGame SOURCES ${CPP_SOURCES} FONT)
