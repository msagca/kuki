# My Game

A game on the [Kuki](https://github.com/msagca/kuki) engine, which is the submodule
`external/kuki`, built from source with the game.

## Building and running

With the `kuki` command, which the engine's installer puts on the PATH. Build the game
(`kuki build --release` for a release build):

```bash
kuki build
```

Run it; any arguments after `run` go to the game (`kuki run --help` lists them):

```bash
kuki run
```

Move the engine to its latest main, then build, run and commit:

```bash
kuki update
```

Or by hand: `cmake --workflow Debug`, and the executable is in `build/game/` (in a folder per
configuration with Visual Studio). After a fresh clone, `git submodule update --init --recursive`
first. Git LFS is needed for the engine's binary assets, and the Windows SDK for the HLSL compiler.

## What goes where

- `CMakeLists.txt` -- adds the engine, then the game.
- `game/CMakeLists.txt` -- `kuki_add_game(<target> SOURCES ... [ASSETS <dir>...] [FONT])`, plus
  anything particular to the game, such as `kuki_add_blender_models`.
- `game/include/my_game_world.hpp` -- the game's components and archetypes, and its `World`.
- `game/include/my_game.hpp`, `game/src/my_game.cpp` -- the application, its systems in order,
  and `SetupMyGame`, which registers the controls, makes the assets and builds the scene.
- `game/src/spin_system.cpp` -- the game's behaviour, which is always a system's.
