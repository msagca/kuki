project(KukiFrontier)
file(
  GLOB_RECURSE CPP_SOURCES
  CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
# Everything in the game is made in code -- the map, the props, the buildings, the birds -- so the
# font is all it stages.
kuki_add_game(${PROJECT_NAME} SOURCES ${CPP_SOURCES} FONTS)
# The map, which needs no window to generate: what it promises -- the rules kept everywhere, the
# layers agreeing with the ground, one seed giving one map -- is tested here. See
# `kuki_add_game_tests`.
file(
  GLOB TEST_SOURCES
  CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
)
kuki_add_game_tests(
  ${PROJECT_NAME}
  SOURCES ${TEST_SOURCES} "${CMAKE_CURRENT_SOURCE_DIR}/src/terrain.cpp"
)
