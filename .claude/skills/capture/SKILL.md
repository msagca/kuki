---
name: capture
description: Build a Kuki game or the editor and look at a frame it draws, through --capture, or record a video of it, through --record. Use to see what a rendering, shader, scene or game change looks like, to compare before and after, or when asked to run, screenshot or record a game -- no one needs to be at the screen.
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

## Record a video

What lives in motion -- a blur as the camera moves, wind in the grass, impact frames and smears --
is judged from a video. `--record <file.mp4>` records `--record-frames <n>` frames (600, ten
seconds) and quits:

```bash
build/game/frontier/KukiFrontier.exe --record "<scratchpad>/frontier.mp4" --record-frames 180 --seed 7 --demo
```

- It needs `ffmpeg` on the `PATH` (piped to, never linked); without it the run says so and exits 1.
  So does one that records fewer frames than asked, unless the window was closed on it.
- Each frame is exactly one tick (`ApplicationInternal::SetFixedStep`), so the video plays at the
  game's speed however slowly it is made: every frame is read back with a stall, about 85 ms at
  1440p in Debug. A three-second clip takes about half a minute; give the run a timeout to match.
- It starts once the targets are the window's size, a few frames in, and stops early if the window
  is resized: a raw stream cannot change size.
- The mouse is live: a cursor left over the window shows whatever it hovers.

The Read tool cannot open a video. To look at one, pull frames out with ffmpeg and read the PNG:

```bash
ffmpeg -v error -y -i "<scratchpad>/frontier.mp4" -vf "select='eq(n\,0)+eq(n\,90)+eq(n\,179)',scale=640:-1,tile=3x1" -frames:v 1 "<scratchpad>/frames.png"
```
