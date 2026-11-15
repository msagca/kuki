# CLAUDE.md

My Game, a game on the Kuki engine. The engine is the submodule external/kuki, added with
add_subdirectory in CMakeLists.txt, and the game is game/, declared with kuki_add_game (the engine's
cmake/KukiGame.cmake).

The engine's rules hold here as in its own games: read external/kuki/CLAUDE.md before a change, and
external/kuki/engine/CLAUDE.md for the renderer. Its skills, in external/kuki/.claude/skills/, say
how a binding, a component or a graphics setting is added.

- `kuki build` builds (`kuki build --release`), `kuki run` runs, and `kuki run --capture <file.png>`
  saves a frame and quits -- how to see what a change draws.
- `kuki update` moves the engine to its latest main and stages it; build and run before committing.
- A change to the engine is made in external/kuki, on a branch of the engine, and goes through the
  engine's own pull requests; this repository then commits the new submodule pointer. What belongs
  to this game alone stays here.
