project(KukiTest)
enable_testing()
include(CTest)
include(GoogleTest)
file(
  GLOB_RECURSE C_SOURCES
  CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)
file(
  GLOB_RECURSE CPP_SOURCES
  CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
add_executable(${PROJECT_NAME} ${C_SOURCES} ${CPP_SOURCES})
if(KUKI_ENABLE_LINTING AND CLANG_TIDY_EXE)
  enable_clang_tidy(${PROJECT_NAME})
endif()
target_include_directories(
  ${PROJECT_NAME}
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(${PROJECT_NAME} PRIVATE KukiEngine gtest)
# The rules of the duel game, compiled in as they are. They know nothing of a window or a scene,
# which is the point of them, so they are tested the way the engine's windowless parts are. The
# game's own library cannot be linked instead: it is only built with the games, and holds the
# scripts and a module entry point as well.
target_sources(
  ${PROJECT_NAME}
  PRIVATE "${CMAKE_SOURCE_DIR}/game/duel/src/duel_sim.cpp"
)
target_include_directories(
  ${PROJECT_NAME}
  PRIVATE "${CMAKE_SOURCE_DIR}/game/duel/include"
)
kuki_stage_runtime(${PROJECT_NAME})
# Discovered when the tests are run and not when the binary links. Discovery runs the binary, and
# in a shared build it does not start until the engine has been staged beside it, which happens
# after the link and not as part of it.
gtest_discover_tests(${PROJECT_NAME} DISCOVERY_MODE PRE_TEST)
install(
  TARGETS ${PROJECT_NAME}
  DESTINATION "${CMAKE_INSTALL_BINDIR}"
  COMPONENT ${PROJECT_NAME}
)
if(UNIX AND NOT APPLE)
  set_target_properties(
    ${PROJECT_NAME}
    PROPERTIES INSTALL_RPATH "$ORIGIN" BUILD_WITH_INSTALL_RPATH TRUE
  )
endif()
