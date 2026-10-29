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
# The frontier's map, for the same reason: generating it needs no window, and what it promises --
# the rules kept everywhere, the layers agreeing with the ground -- is worth a test of its own.
target_sources(
  ${PROJECT_NAME}
  PRIVATE "${CMAKE_SOURCE_DIR}/game/frontier/src/terrain.cpp"
)
target_include_directories(
  ${PROJECT_NAME}
  PRIVATE "${CMAKE_SOURCE_DIR}/game/frontier/include"
)
kuki_stage_runtime(${PROJECT_NAME})
# Mistakes the engine promises to refuse at compile time: an archetype a world never declared, a
# query no archetype answers, a component an entity was not made with, two flags of one name. A
# promise like that is only kept if something breaks when it stops being, and the thing that breaks
# has to be a build. So each of these sources is a target of its own, left out of the build, and a
# test builds it and passes only if the compiler's output carries the message the source's first
# line expects -- the engine's own `static_assert`. A source that compiled fails its test, and so
# does one that failed for some other reason, which is what a missing include would look like.
#
# Serial, because each one runs the build tool in this tree, and two at once would fight over it.
file(
  GLOB COMPILE_FAIL_SOURCES
  CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/compile_fail/*.cpp"
)
foreach(source ${COMPILE_FAIL_SOURCES})
  get_filename_component(name "${source}" NAME_WE)
  set(target KukiCompileFail_${name})
  add_library(${target} OBJECT EXCLUDE_FROM_ALL "${source}")
  target_link_libraries(${target} PRIVATE KukiEngine)
  file(STRINGS "${source}" expected LIMIT_COUNT 1 REGEX "^// expect: ")
  string(REPLACE "// expect: " "" expected "${expected}")
  add_test(
    NAME CompileFail.${name}
    COMMAND
      ${CMAKE_COMMAND} --build "${CMAKE_BINARY_DIR}" --target ${target} --config
      $<CONFIG>
  )
  set_tests_properties(
    CompileFail.${name}
    PROPERTIES PASS_REGULAR_EXPRESSION "${expected}" RUN_SERIAL TRUE
  )
endforeach()
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
