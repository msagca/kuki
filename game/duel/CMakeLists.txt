project(KukiDuel)
file(
  GLOB_RECURSE CPP_SOURCES
  CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
# The text on screen is drawn in the font. See `kuki_add_game`.
kuki_add_game(${PROJECT_NAME} SOURCES ${CPP_SOURCES} FONT)
# The fighter, rigged and animated in one .blend and exported to `model/` beside the executable as
# one glTF with its skeleton and every clip. `tools/make_fighter.py` is what wrote the .blend.
kuki_add_blender_models(
  ${PROJECT_NAME}
  "${CMAKE_CURRENT_SOURCE_DIR}/src/model/fighter.blend"
  model
)
# The four stages, one .glb a part, exported to `stage/` beside the executable. `tools/make_stages.py`
# is what wrote the .blend.
kuki_add_blender_models(
  ${PROJECT_NAME}
  "${CMAKE_CURRENT_SOURCE_DIR}/src/model/stages.blend"
  stage
)
