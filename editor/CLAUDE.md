# The editor

It opens, edits, saves, loads and plays its own scenes, with fewer features than it had. Its
`EditorWorld` (`editor.hpp`) is the engine's archetypes, its settings entity and the courtyard
scene's sky. What went with scripts and fixed archetypes: adding and removing components (the
Hierarchy's New menu makes an entity of a chosen kind), the script panel, and hosting games (`--game`
only warns). The camera controller is an editor system (`CameraController`, listed before
`RenderingSystem`), set in the Settings panel.

It starts `Stopped`: no ticks, and an animator previews its clip by frame time. Play (`BeginPlay`,
F5 or `--play`) saves the scene to a temporary file and sets `Playing`; Stop (`EndPlay`, F5) loads
it back. F6 pauses, F10 steps one tick.

What is left of hosting: `FindGames` and `KUKI_GAME_MODULE` still describe a game, and
`Application`'s `hosting` flag makes `Quit()` during play stop instead of close. Nothing sets it now.

`Editor::IsSupported` maps a component to the `RendererCapabilities` flag it needs, so the panels
hide what the backend cannot draw -- add a case with any backend-specific component.
