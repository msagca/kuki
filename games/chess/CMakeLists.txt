project(KukiChess)
file(
  GLOB_RECURSE CPP_SOURCES
  CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
# The board's rank and file labels are baked from the font at startup; the materials are loaded
# by name from `material/` beside the executable. The engine and nothing else is linked.
kuki_add_game(
  ${PROJECT_NAME}
  SOURCES ${CPP_SOURCES}
  ASSETS "${CMAKE_CURRENT_SOURCE_DIR}/src/material"
  FONTS
)
# Pieces, board and clock, modelled in one .blend and exported to `model/` beside the executable
# as one glTF a mesh. `Chess::Start` loads them by those names.
kuki_add_blender_models(
  ${PROJECT_NAME}
  "${CMAKE_CURRENT_SOURCE_DIR}/src/model/chess.blend"
  model
)
