---
name: capture
description: Build a Kuki game or the editor and look at a frame it draws, through --capture. Use to see what a rendering, shader, scene or game change looks like, to compare before and after, or when asked to run or screenshot a game -- no one needs to be at the screen.
---

# Capture a frame

`--capture <file.png>` draws `--capture-frame <n>` frames (300 by default, about five seconds at
sixty), saves the last one and quits.

## 1. Build the target and its runtime

| What | Target | Executable |
|---|---|---|
| chess | `KukiChess` | `build/game/chess/KukiChess.exe` |
| duel | `KukiDuel` | `build/game/duel/KukiDuel.exe` |
| frontier | `KukiFrontier` | `build/game/frontier/KukiFrontier.exe` |
| editor | `KukiEditor` | `build/editor/KukiEditor.exe` |

Build the target **and** `<Target>Runtime`. The game alone leaves the DLLs from the last build
beside it, and the game then runs against the engine from before -- the change you want to see is
not in the picture.

```bash
cmake --build --preset Debug --target KukiFrontier KukiFrontierRuntime
```

If `build/` is not configured, `cmake --preset Debug` first. The build prints the executable's path
at its end; trust that over the table.

## 2. Run it

Give an absolute path in the session's scratchpad, never one in the repository, and a timeout: the
run is a few seconds plus shader compilation.

```bash
build/game/frontier/KukiFrontier.exe --capture "<scratchpad>/frontier.png" --seed 7 --demo
```

What sets up the picture (`--help` on any executable lists everything it reads):

- frontier: `--seed <n>` (same seed, same map), `--layer cells|resources|movement|building`,
  `--distance <6..70>` (camera distance), `--demo` (a path and a few buildings).
- duel: `--stage alley|temple|rooftop|shrine`, `--no-flash`, `--no-ik`. It opens on its select
  screen, which is what a capture shows.
- editor: `--play` to capture the scene playing rather than stopped.
- any: `--capture-frame <n>` for a later or earlier frame -- later for something that settles
  (probes, clouds), earlier to catch a start-up state.
- any game: `--settings game|graphics|controls` to capture the settings menu open on that page.

## 3. Look

Open the PNG with the Read tool. If there is no file, or it is empty, read the run's log: the usual
causes are `dxcompiler.dll`/`dxil.dll` missing beside the executable (nothing compiles, so nothing
is drawn), no Direct3D 12 device (the run stops at startup), and a shader that failed to compile,
which is reported at run time, not at build time.

To compare, capture before and after with the same flags and frame, and name the files for what
they show.
