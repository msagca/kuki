# The editor

**The editor runs with fewer features than it had.** It opens, edits, saves and loads its own
scenes, and plays them (the animators' states and physics tick). It declares its own `EditorWorld`
(`editor.hpp`): the engine's archetypes, its settings entity and the courtyard scene's sky. What went
with scripts and fixed archetypes: adding and removing components (the Hierarchy's New menu makes an
entity of a chosen kind instead), the script panel, and hosting games -- a game's behaviour is its
own systems, part of its application's type, so `--game` only warns (`main.cpp`). The camera
controller is an editor system (`CameraController`, listed before `RenderingSystem`), its settings
in the Settings panel.

The editor starts `Stopped`: ticks do not run, and an animator plays its clip by frame time as a
preview. Play (`BeginPlay`) saves the scene to a temporary file and sets `Playing`; Stop (`EndPlay`)
loads that file back. F5 plays and stops, F6 pauses, F10 steps one tick; `--play` starts playing.

What is left of hosting: `FindGames` and `KUKI_GAME_MODULE` (`game_module.hpp`, `macros.hpp`) still
describe a game to whatever would host it, and `Application` keeps a `hosting` flag under which
`Quit()` during play stops instead of closing. Nothing in the editor sets it now.

`Editor::IsSupported` maps a component to the `RendererCapabilities` flag it needs, so the panels
hide what the running backend cannot draw -- add a case when adding a backend-specific component.
