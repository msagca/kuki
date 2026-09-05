# Every game lives in its own directory under `game/` and is picked up automatically: a directory
# here with a `CMakeLists.txt` in it is a game, and nothing has to be added to this file to build a
# new one. Globbed with CONFIGURE_DEPENDS so that adding one reconfigures rather than being missed.
#
# Reached from the root only when KUKI_BUILD_GAMES is on, so the ordinary editor build pays nothing
# for games it is not building.
file(GLOB GAME_ENTRIES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*")
set(KUKI_GAME_TARGETS "")
foreach(GAME_ENTRY ${GAME_ENTRIES})
  if(IS_DIRECTORY "${GAME_ENTRY}" AND EXISTS "${GAME_ENTRY}/CMakeLists.txt")
    get_filename_component(GAME_NAME "${GAME_ENTRY}" NAME)
    add_subdirectory("${GAME_ENTRY}")
    list(APPEND KUKI_GAME_TARGETS ${GAME_NAME})
  endif()
endforeach()
if(KUKI_GAME_TARGETS)
  message(STATUS "Games: ${KUKI_GAME_TARGETS}")
else()
  message(
    WARNING
    "KUKI_BUILD_GAMES is on but no game directory under game/ has a CMakeLists.txt"
  )
endif()
