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
# glfw for `input_trigger_test.cpp`, which writes triggers in GLFW's numbers as saved preferences do.
# nlohmann_json for `transform_test.cpp`, which reads JSON through the engine's serializers
# (`serdes.hpp`); the engine links it privately, so it does not come with `KukiEngineInternal`.
target_link_libraries(
  ${PROJECT_NAME}
  PRIVATE KukiEngineInternal glfw gtest nlohmann_json
)
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
# Every installed header in one translation unit, compiled with what a game is given and nothing
# more: `KukiEngine` alone, so `engine/internal/` is not on the include path and glfw's headers are
# not either. It fails to build if a public header includes an internal one, or names something
# from a library a game does not link -- either of which would build here, where everything is on
# the path, and break the first game built against a release. Built with the tests, which is where
# CI builds it.
file(
  GLOB KUKI_PUBLIC_HEADERS
  CONFIGURE_DEPENDS
  "${CMAKE_SOURCE_DIR}/engine/include/*.hpp"
)
set(KUKI_PUBLIC_HEADERS_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/public_headers.cpp")
set(KUKI_PUBLIC_HEADERS_TEXT "")
foreach(header ${KUKI_PUBLIC_HEADERS})
  get_filename_component(name "${header}" NAME)
  string(
    APPEND KUKI_PUBLIC_HEADERS_TEXT
    "#include <${name}>
"
  )
endforeach()
file(
  CONFIGURE
  OUTPUT "${KUKI_PUBLIC_HEADERS_SOURCE}"
  CONTENT "${KUKI_PUBLIC_HEADERS_TEXT}"
)
add_library(KukiPublicHeaders OBJECT "${KUKI_PUBLIC_HEADERS_SOURCE}")
target_link_libraries(KukiPublicHeaders PRIVATE KukiEngine)
add_dependencies(${PROJECT_NAME} KukiPublicHeaders)
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
# The names the CLAUDE.md files and skills give, each looked for in the sources; see the script. A
# test rather than a CI step so a local ctest runs it too, and skipped, with a note, where there is
# no Python to run it with.
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
  add_test(
    NAME Docs.NamesExist
    COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/check_docs.py"
  )
else()
  message(STATUS "No Python, so Docs.NamesExist is not registered")
endif()
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
