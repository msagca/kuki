---
name: add-game
description: Checklist for adding a new game to Kuki under games/ -- the CMake target, the world and its components, the application class, the setup function and module macros, the game's systems, its CLAUDE.md, its rules in the tests, and the first build and capture. Use whenever a new game directory is started.
---

# Add a game

The frontier (`games/frontier/`) is the one to copy: everything in it is made in code, which is how
a new game should start. Below, `<name>` is the directory and namespace (`frontier`), `<Name>` the
class (`Frontier`).

A game kept in a repository of its own starts with `kuki new <game_name>` instead (`cli/`): the
repository, the engine pinned as its submodule, and the template renamed for the game, already
building. The steps below are the same there, with `game/` in place of `games/<name>/`; the rules
(step 6) are tested by the same call, once the repository's own `CMakeLists.txt` turns
`KUKI_BUILD_TESTS` on and calls `enable_testing()`.

1. `games/<name>/CMakeLists.txt` -- `project(Kuki<Name>)`, a glob of `src/*.cpp`, and
   `kuki_add_game(${PROJECT_NAME} SOURCES ${CPP_SOURCES} FONTS)` (`cmake/KukiGame.cmake`), which
   makes the library and the launcher, takes `include/` as the game's headers, and stages the
   DLLs, the HLSL compiler -- without it nothing is drawn -- and the font beside the executable.
   `FONTS` if anything is written on screen, the settings menu included; `ASSETS <dir>` for a
   directory loaded at run time (chess's `material`). Anything else about the game's code goes on
   `${KUKI_GAME_CODE}`, not on the executable. It is found without registration
   (`games/CMakeLists.txt` globs for it). Link nothing but the engine.
2. `games/<name>/include/<name>_world.hpp` -- the game's archetypes and `using <Name>World = kuki::World<...>`,
   all in `namespace <name>`. A game's component is a plain struct, declared with
   `KUKI_COMPONENT(<name>::Type, index)` at global scope after its definition, indices from 0 and
   the game's own (`frontier_world.hpp`). The engine's archetypes (`CameraObject`, `MeshObject`,
   `LightObject`, ...) come with every world and are not listed.
3. `games/<name>/include/<name>.hpp` -- the application class,
   `SystemApplication<<Name>World, <Name>'s systems..., kuki::RenderingSystem>`, in the order they
   are to run (each system's `Tick`/`Update` in list order; `RenderingSystem` last), and
   `Setup<Name>(kuki::Application &)`, which `Start` calls and `KUKI_GAME_MODULE` names. Every
   header and source of the game is in `namespace <name>`, the application and systems as much as
   the world and rules; only `KUKI_COMPONENT` and the module macros stand outside it, at global
   scope. Forward declarations of the engine's (`namespace kuki { class Application; }`) go above
   the game's namespace, not in it, or they declare a `<name>::kuki` nobody defines.
4. `games/<name>/src/<name>.cpp` -- the constructor
   (`SystemApplication(ApplicationDescription{.name = "Kuki <Name>"})`), `Start` calling
   `Setup<Name>(*this)` and nothing else, and `Setup<Name>`: register the controls first (the
   `add-binding` skill), load or build the assets, then the scene with
   `<Name>World::Game(app, "<Name>").Scene("Main")`, a `CameraObject` included or nothing is drawn.
   At the end of this one file, once, after the game's namespace is closed:
   `KUKI_GAME_MODULE("<Name>", <name>::Setup<Name>)` and `KUKI_GAME_MAIN(<name>::<Name>)`.
5. The game's behaviour is its systems, one class each (`kuki::System`, constructed as
   `System(std::in_place_type<S>, app)`), beginning in `PostStart`. Rules go in `Tick`,
   presentation in `Update`. Command-line flags are a system's `static constexpr std::array FLAGS`.
6. Rules that decide anything -- the state a tick changes, a map from a seed -- go in a windowless
   file of their own (`terrain.cpp`, `duel_sim.cpp`) with no `Application` in it, and are tested
   beside the game: gtest cases in `games/<name>/tests/`, and in the game's `CMakeLists.txt`
   `kuki_add_game_tests(${PROJECT_NAME} SOURCES <the tests> <the rules' sources>)`, which makes
   `Kuki<Name>Test` when the tests are on (the frontier's is the one to copy). `tests/` is the
   engine's and the CLI's, and names no game.
7. `games/<name>/CLAUDE.md` -- what the game is, what is in it and where, its frame budget, and the
   flags that set up what a `--capture` shows. How things are, not plans: those are issues.
8. Build the game and its runtime together, or it runs against an old engine DLL:
   `cmake --build --preset Debug --target Kuki<Name> Kuki<Name>Runtime`. Then look at it with the
   `capture` skill, and play it from the keyboard alone -- every action a binding.
