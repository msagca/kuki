# A game on Kuki

The start of a game in a repository of its own, built with the engine pinned as a git submodule.
It is a box turning on a lawn: one component of the game's own (`Spin`), the system that turns it
(`SpinSystem`, in `Tick`), one rebindable binding (R turns it the other way) and a scene made in
code. Escape opens the settings menu that every game has.

## Starting a repository from it

```bash
git init my-game && cd my-game
cp -r <kuki>/template/. .
git submodule add https://github.com/msagca/kuki external/kuki
git submodule update --init --recursive   # the engine's own submodules too
git lfs install                            # the engine's binary assets (the font) are in LFS
```

Then rename what says `MyGame`, `my_game` and `KukiTemplate`: the project in `CMakeLists.txt`,
the target in `game/CMakeLists.txt`, the namespace, the application class, `SetupMyGame` and the
headers. Copy kuki's `.clang-format` beside this file if the code is to be formatted the way the
engine's is.

## Building and running

```bash
cmake --workflow Debug        # or Release
build/game/KukiTemplate.exe    # build/game/Debug/ with the Visual Studio generator
```

The executable lands in `build/game/` (in a folder per configuration with Visual Studio) beside
the engine's DLLs, the HLSL compiler and the font, all staged by `kuki_add_game`. Direct3D 12 is
the only backend, and the Windows SDK has to be installed for the compiler to be found. The
engine's own flags work here as in any of its games: `--help` lists them, and `--capture <file.png>` saves a frame and quits.

## Updating the engine

The engine is built from `external/kuki` with the game, so a change made there is one rebuild
away. To move to a newer engine:

```bash
git -C external/kuki pull
git -C external/kuki submodule update --init --recursive
git add external/kuki && git commit -m "update the engine"
```

## What goes where

- `CMakeLists.txt` -- the root: adds the engine, then the game. Kuki builds only its engine when it
  is not the top-level project.
- `game/CMakeLists.txt` -- `kuki_add_game(<target> SOURCES ... [ASSETS <dir>...] [FONT])`, plus
  anything particular to the game, such as `kuki_add_blender_models`. See kuki's
  `cmake/KukiGame.cmake`.
- `game/include/my_game_world.hpp` -- the game's components and archetypes, and its `World`.
- `game/include/my_game.hpp`, `game/src/my_game.cpp` -- the application, its systems in order,
  and `SetupMyGame`, which registers the controls, makes the assets and builds the scene.
- `game/src/spin_system.cpp` -- the game's behaviour, which is always a system's.

The rules the engine holds a game to -- plain-data components, behaviour in systems, every action
a binding, deterministic ticks -- are in kuki's `CLAUDE.md`, and its `add-game` and `add-binding`
skills say how each part is added.
