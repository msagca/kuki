# Every game lives in its own directory under `game/` and is picked up automatically: a directory
# here with a `CMakeLists.txt` in it is a game, and nothing has to be added to this file to build a
# new one. Globbed with CONFIGURE_DEPENDS so that adding one reconfigures rather than being missed.
#
# Reached from the root only when KUKI_BUILD_GAMES is on, so the ordinary editor build pays nothing
# for games it is not building.
#
# One thing a new game does have to say for itself: `kuki_report_binary(${PROJECT_NAME})`, which
# prints where its executable landed. That cannot be applied from here -- a custom command may only
# be attached to a target in the directory that created it.
file(GLOB GAME_ENTRIES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*")
set(KUKI_GAME_TARGETS "")
foreach(GAME_ENTRY ${GAME_ENTRIES})
  if(IS_DIRECTORY "${GAME_ENTRY}" AND EXISTS "${GAME_ENTRY}/CMakeLists.txt")
    add_subdirectory("${GAME_ENTRY}")
    # Asked of the directory rather than taken from its name, so what is listed is the target a
    # build would actually be asked for -- `KukiChess` rather than `chess`.
    get_property(
      GAME_ENTRY_TARGETS
      DIRECTORY "${GAME_ENTRY}"
      PROPERTY BUILDSYSTEM_TARGETS
    )
    foreach(GAME_TARGET ${GAME_ENTRY_TARGETS})
      get_target_property(GAME_TARGET_TYPE ${GAME_TARGET} TYPE)
      if(GAME_TARGET_TYPE STREQUAL "EXECUTABLE")
        list(APPEND KUKI_GAME_TARGETS ${GAME_TARGET})
      endif()
    endforeach()
  endif()
endforeach()
# Handed back so the installer can offer each game separately. A subdirectory's variables are its
# own, so without this the root would have to be told about every game that is added -- which is
# the one thing this directory exists to avoid.
set(KUKI_GAME_TARGETS ${KUKI_GAME_TARGETS} PARENT_SCOPE)
if(KUKI_GAME_TARGETS)
  message(STATUS "Games: ${KUKI_GAME_TARGETS}")
else()
  message(
    WARNING
    "KUKI_BUILD_GAMES is on but no game directory under game/ has a CMakeLists.txt"
  )
endif()
