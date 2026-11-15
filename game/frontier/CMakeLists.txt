project(KukiFrontier)
file(
  GLOB_RECURSE CPP_SOURCES
  CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
# Everything in the game is made in code -- the map, the props, the buildings, the birds -- so the
# font is all it stages.
kuki_add_game(${PROJECT_NAME} SOURCES ${CPP_SOURCES} FONT)
