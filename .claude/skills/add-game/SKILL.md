---
name: add-game
description: Checklist for adding a new game to Kuki under game/ -- the CMake target, the world and its components, the application class, the setup function and module macros, the game's systems, its CLAUDE.md, its rules in the tests, and the first build and capture. Use whenever a new game directory is started.
---

# Add a game

The frontier (`game/frontier/`) is the one to copy: everything in it is made in code, which is how
a new game should start. Below, `<name>` is the directory and namespace (`frontier`), `<Name>` the
class (`Frontier`).

1. `game/<name>/CMakeLists.txt` -- copy the frontier's and rename `project(Kuki<Name>)` and the
   staging comment. It is found without registration (`game/CMakeLists.txt` globs for it). Keep
   `kuki_add_game` (the library and launcher; include directories go on `${KUKI_GAME_CODE}`, not
   on the executable), `kuki_report_binary`, the font staging if anything is written on screen,
   and the dxcompiler staging -- without it nothing is drawn. Link nothing but the engine.
2. `game/<name>/include/<name>_world.hpp` -- the game's archetypes and `using <Name>World = kuki::World<...>`,
   all in `namespace <name>`. A game's component is a plain struct, declared with
   `KUKI_COMPONENT(<name>::Type, index)` at global scope after its definition, indices from 0 and
   the game's own (`frontier_world.hpp`). The engine's archetypes (`CameraObject`, `MeshObject`,
   `LightObject`, ...) come with every world and are not listed.
3. `game/<name>/include/<name>.hpp` -- the application class,
   `SystemApplication<<name>::<Name>World, <Name>'s systems..., kuki::RenderingSystem>`, in the
   order they are to run (each system's `Tick`/`Update` in list order; `RenderingSystem` last), and
   `Setup<Name>(kuki::Application &)`, which the editor calls to load the game into itself.
4. `game/<name>/src/<name>.cpp` -- the constructor
   (`SystemApplication(ApplicationDescription{.name = "Kuki <Name>"})`), `Start` calling
   `Setup<Name>(*this)` and nothing else, and `Setup<Name>`: register the controls first (the
   `add-binding` skill), load or build the assets, then the scene with
   `<Name>World::Game(app, "<Name>").Scene("Main")`, a `CameraObject` included or nothing is drawn.
   At the end of this one file, once: `KUKI_GAME_MODULE("<Name>", Setup<Name>)` and
   `KUKI_GAME_MAIN(<Name>)`.
5. The game's behaviour is its systems, one class each (`kuki::System`, constructed as
   `System(std::in_place_type<S>, app)`), beginning in `PostStart`. Rules go in `Tick`,
   presentation in `Update`. Command-line flags are a system's `static constexpr std::array FLAGS`.
6. Rules that decide anything -- the state a tick changes, a map from a seed -- go in a windowless
   file of their own (`terrain.cpp`, `duel_sim.cpp`) with no `Application` in it, and are compiled
   into the tests: a `target_sources` and `target_include_directories` pair in `test/CMakeLists.txt`
   beside the frontier's, and a test in `test/src/`.
7. `game/<name>/CLAUDE.md` -- what the game is, what is in it and where, its frame budget, and the
   flags that set up what a `--capture` shows. How things are, not plans: those are issues.
8. Build the game and its runtime together, or it runs against an old engine DLL:
   `cmake --build --preset Debug --target Kuki<Name> Kuki<Name>Runtime`. Then look at it with the
   `capture` skill, and play it from the keyboard alone -- every action a binding.
