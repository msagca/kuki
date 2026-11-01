# CLAUDE.md

Guidance for Claude Code in this repository. This file holds what nearly every change needs; the
rest is read where it is used:

- `editor/`, `game/duel/`, `game/frontier/` and `engine/src/shader/` have a `CLAUDE.md` of their
  own, loaded with the files there.
- Recipes are skills in `.claude/skills/`: `capture`, `add-engine-component`, `add-render-pass`,
  `add-compile-fail-check`.
- Hooks (`.claude/settings.json`) clang-format only the changed lines of an edited C++ file (some
  files have drifted from `.clang-format`), format CMake with gersemi, and block edits to `external/`.

## Build, run, test

```bash
git submodule update --init --recursive                # external/ is required
cmake --workflow Release                               # configure + build + test + package
cmake --workflow Debug                                 # configure + build + test
cmake --preset Debug && cmake --build --preset Debug   # iterate
cmake --preset Debug -DKUKI_BUILD_TESTS=ON && cmake --build --preset Debug
ctest --test-dir build --output-on-failure [-R <TestName>]   # one test per gtest case
```

Presets: `Debug`, `Release`, `MSVC` (built with `MSVC-Debug`/`MSVC-Release`), `Clang-Debug`/
`Clang-Release` (Ninja). All share `build/`, so switching reconfigures in place. Games are on and
tests off (`KUKI_BUILD_TESTS`); the tests cover what has no window -- the tick, gestures, the ECS,
rollback, networking, the duel's rules. A `test/compile_fail/*.cpp` passes only if the compiler
prints its expected message on the first line. `-DKUKI_ENABLE_LINTING=ON` attaches clang-tidy.

Executables land in `build/editor/`, `build/game/<name>/`, `build/test/`. A game's folder gets fresh
DLLs from its `<Target>Runtime` target: build both, or the game runs against the old engine.
`--capture <file.png>` saves the frame after `--capture-frame <n>` (300) and quits -- how to see
what a build draws (the `capture` skill). Assets are staged beside the binary and found through
`ResolvePath` against the executable's directory, never the working directory.

Direct3D 12 is the only backend; without it the application stops at startup. HLSL is compiled at
run time by `dxcompiler.dll`/`dxil.dll`, found in the Windows SDK at configure time and copied
beside each executable -- without them nothing is drawn.

## Layout

- `engine/` -- `KukiEngine`, linked by everything. Public headers in `include/`, included by bare
  name (`#include <entity_manager.hpp>`).
- `editor/` -- `KukiEditor` (Dear ImGui), the only target linking imgui/imguizmo/file-browser.
- `game/` -- a subdirectory with a `CMakeLists.txt` is a game, found without registration. It links
  `KukiEngine` alone and calls `kuki_report_binary(${PROJECT_NAME})`.
- `test/` -- gtest, with the duel's rules and the frontier's map compiled in.
- `external/` -- submodules, never patched; their quirks are worked around, and explained, in the
  root `CMakeLists.txt`.

## Design principles

They settle a choice before the architecture below does. A change that breaks one says why in its
commit. Where the repo falls short of one, the gap is named.

- **Data first.** A component is plain, trivially copyable data -- no virtuals, owning pointers or
  containers -- so a table moves, snapshots and rolls back its rows as bytes. What varies in length
  is an asset's or a system-owned store's, reached by a handle. Gap: `Animator` and `Skeleton`, the
  only exemptions; once they are reworked a `static_assert` refuses every component that is not
  trivially copyable.
- **Behaviour is a system's.** Whatever runs -- every tick, every frame, on input -- is a `System`,
  working over tables through queries. A component has no method that changes it; a function that
  only computes from a component's data is a free function. An `Application` subclass only builds
  its scene in `Start`. Gap: `Camera`'s and `Light`'s methods, and `Animator`'s until it is reworked.
- **Poll state, do not subscribe to events.** A system reads state on its own schedule: flags,
  counters, masks, a table's `version`, a queue it drains itself. What happened is recorded as data
  for whoever cares to read it (`ButtonMask`, an animator's fired events), not announced to
  subscribers -- a web of handlers runs in an order nobody chose and is hard to follow in a
  debugger. A callback may only compute a value its caller asks for (ground height, a seat's
  buttons, a setting's get and set), never report that something happened. Gap:
  `RegisterInputAction`'s callbacks, used by chess and the editor's camera.
- **Data lives where it is used.** Upload to the GPU when data changes, not every frame; compute on
  the GPU what only the picture needs; copy columns as they lie (a pool's `WorldMatrix`); key cached
  GPU work on a table's `version` or an asset's `revision`. A readback says beside it why.
- **Performance before looks.** An effect's cost is measured (`KUKI_PROFILE_SCOPE`) and stated in
  its commit; one that is not cheap is a graphics setting with a `RendererCapabilities` flag. Against
  the frame budget -- each game's own, in its `CLAUDE.md` -- the effect gives way.
- **Low-poly by default**: flat-shaded, built with `MeshBuilder` or a script, unless a game's
  `CLAUDE.md` says otherwise, and coloured by its vertices rather than textures. Gap: `Vertex` has
  no colour yet, so colour is the material's.
- **Variety from instances, not variants.** Vary the transforms and per-instance parameters (a tint)
  of one mesh and material -- one pool, one instanced draw -- rather than adding meshes or materials,
  each a pool and a draw of its own. Gap: an instance has only its `WorldMatrix`, and skinned meshes
  are drawn one at a time.
- **The engine keeps what more than one game could use**, in `engine/` and `namespace kuki`; a
  game's folder holds that game alone, in a namespace of its own. Code found general in a game is
  moved, not copied. Gap: the games' systems are global, and the frontier's `CameraSystem` belongs in
  the engine.
- **Next to no UI over the game.** Only what a player would decide wrongly without, and the settings
  menu cannot hold, is shown: in the world where it can be (a building's outline on its cells), over
  the picture only where not (the state of a fight). Options, legends, debug views, merely
  interesting readouts and key hints are settings toggles or command-line flags.
- **Every game plays from the keyboard alone.** Every action is a named binding
  (`RegisterBinding`/`RegisterButton`); the mouse is a second way, never the only one. Gap: chess
  (#16) and the frontier (#17); nothing enforces it (#18).
- **Rules are deterministic and windowless.** A game's rules are its `Tick`, in files compiled into
  the tests (`duel_sim.cpp`, `terrain.cpp`): the same state and buttons, or seed, give the same
  result on every machine, which rollback and shared maps stand on. No clock, `<random>`
  distribution, unordered iteration or anything drawn reaches a rule.
- **Refuse mistakes at compile time**, by a `static_assert` or constraint whose message says what to
  do instead, each with a check in `test/compile_fail/`.
- **Content is made by code**: `MeshBuilder`, the generators, or a script in the repo run by hand
  (`tools/make_fighter.py`, `tools/make_stages.py`), not modelled by hand -- a change is a readable
  diff, variety is a seed, and LFS holds only what a script wrote.
- **No heap allocation on the hot path** (`Tick`, `Update`, `LateUpdate`) once a scene runs: buffers
  are sized at start and reused. Gap: `Animator`'s event names, made every tick.

## Architecture

### Application and systems

`Application` (`application.hpp`) is the facade -- window, input, assets, scenes, preferences,
overlay -- with `Run()` public and `Start`/`Update`/`Shutdown`/`Status` as protected hooks.
`SystemApplication<World, Systems...>` names an application's archetypes and its systems in order:
`SystemApplication<DuelWorld, AnimationSystem, NetworkSystem, DuelSystem, RenderingSystem>`.
`RenderingSystem` is required. Every system's `Start` runs before the application's (where a game
loads assets and builds its scene) and every `PostStart` after it, which is where a game's own
system begins. There are no scripts: a game's behaviour is its own `System`.

Command-line flags are declared: a system lists `static constexpr std::array FLAGS` of `Flag`
(`command_line.hpp`), and `SystemApplication::FLAGS` joins them with `ENGINE_FLAGS`, refusing a
repeated name at compile time. Read them with `HasFlag`/`GetFlagValue` on
`GetCommandLineArguments()`; `--help` lists them, and an undeclared argument is warned about.

### Tick, input and animation

A frame calls `System::Tick` zero or more times, each standing for `Application::TickTime()` (1/60 s
by default, `tick_clock.hpp`), then `Update` once with the frame's length, then `LateUpdate` before
anything is drawn. Rules go in `Tick`; presentation in `Update`, blending by `TickAlpha()`.

A tick reads the player as a `ButtonMask` (`GetButtons()`). A button is a rebindable binding from
`RegisterButton`, its bit the registration order, latched so a tap shorter than a tick still shows.
`GestureRecognizer` (`gesture.hpp`) turns masks into presses, chords, holds, releases and sequences
in a small notation (`"left right punch"`, `"guard*30 -guard"`); it keeps nothing but the masks, so
fed a peer's it reproduces the peer's gestures. `RegisterInputAction` is unrelated: untimed typed
sequences for editor shortcuts.

An `Animator` without `states` plays one clip by frame time (the editor's preview). With states it
is stepped by the tick (`StepAnimator`): a state plays a clip, fires named events, and goes to its
`next` when a clip that does not loop ends; the game changes state with `Play`. `rootNode`'s travel
is taken out of the pose and reported as `rootDelta`. The pose is sampled in `Update` and
crossfaded. List `AnimationSystem` before a game's system for it to see a tick's events that tick.
IK goals (`IKGoal`: `Reach`, `Aim`; `inverse_kinematics.hpp`) are solved in
`AnimationSystem::LateUpdate`, top of the skeleton down, from the restored pose (`IKHeld`), and are
never saved into it; a goal's `target` is saved as an entity index and resolved after load.

### Settings menu

Every game has a `SettingsMenu` (`settings_menu.hpp`; off in the editor through
`ApplicationDescription::settingsMenu`), opened and closed by Escape, the `Settings` binding,
registered first. Its pages are the game's (`GetSettingsMenu().AddChoice/AddToggle/AddAction(
SettingsPage::Game, ...)`), graphics (saved in `EngineConfig`) and controls (every binding,
rebindable, saved in `Preferences` as `input.<name>`). Quitting is the menu's; a game does not quit
on Escape. While it is open the game hears no input, held input is released, and a game not online
is `Paused` -- a clock in `Update` checks `GetPlayState`. `--settings <page>` opens it for a capture.
Mouse buttons are read directly and cannot be rebound.

### Games and scenes

A game is a library plus the launcher every game shares, `cmake/game_main.cpp` (`kuki_add_game`).
Its sources define `Setup<Game>(Application &)`, `KUKI_GAME_MODULE(name, setup)` and
`KUKI_GAME_MAIN(AppClass)`; a shared build makes `<Target>Game.dll` and `KukiEngine.dll`. Games are
always `Playing`; only the editor stops and pauses (`PlayState`).

Scenes are built with `CreateEntity<A>`/`GetEntityComponent` or the builder in `game_builder.hpp`,
which checks each call against the entity's archetype at compile time
(`ChessWorld::Game(app, "Chess").Scene("Main").Entity<MeshObject>("Pawn").Mesh("Cube")`) and, through
`World::Game`, refuses an archetype the world does not declare.

### ECS

Archetypes are fixed at compile time: `Archetype<Transform, Renderable, Seat>` (`archetype.hpp`),
made of components or bundles (`Renderable`, `Animated` in `engine_archetypes.hpp`); a `Transform`
brings a `WorldMatrix`. An entity keeps its archetype -- **nothing adds or removes a component**. A
component needs `ComponentTraits`: the engine's are in `component_traits.hpp`, a game's come from
`KUKI_COMPONENT(Type, index)` (ids from `FIRST_GAME_COMPONENT`). A game's `World<As...>`
(`world.hpp`) is `EngineArchetypes` plus its own, refuses repeated archetypes and ids, and gives
every scene its archetypes. Removing an entity swaps its table's last row into the gap.

`ForEach<Q...>` visits archetypes with every required component; `Optional<T>` gives a
possibly-null pointer, `Without<T>` excludes. `World::ForEach<Q...>(app, f)` also refuses a query
nothing matches and visits only matching archetypes. `Delete` during a query waits for its end;
`Create` does not, so a callback that creates in the archetype it is visiting must not use what it
was handed afterwards.

**Every drawn entity is in a pool** (`pool.hpp`). `Renderable` is only the tag `Pooled`; the mesh,
material and bounds are the pool's, a `PoolKey` (a mesh or a model's mesh by index, and a material)
with a table of its own. An archetype has one table for entities with no mesh yet and one per pool;
queries visit them all. Create in a pool with `CreateEntity<A>(key)`, move with `MoveEntityToPool`
(the builder's `.Mesh()`/`.Material()`), and give a pool bounds by making it with `CreatePool`
(empty: never culled, and outside the shadow's and probes' fit). A table's `version` rises when a
row comes, goes or moves. Direct3D 12 draws a pool as one instanced draw from its `WorldMatrix`
column and culls per pool; skinned meshes are not culled.

`InstantiateAsset<Root>(name)` places a model: a root entity (`ModelRoot`, or a game's own like the
duel's `Figure`) with `ModelNodeObject`/`MeshObject` children in the model's pools.

`Transform` has no dirty bit and no world matrix: after writing `position`/`rotation`/`scale`, call
`MarkTransformDirty` or the change is never applied, and read where something is from `WorldMatrix`.
The update's invariants are in `entity_manager.hpp`. Scene singletons (`Component::IsSceneSingleton`,
e.g. `IndirectLighting`, `AtmosphereSky`) describe the scene, not the entity holding them.

A game's component is a struct, `KUKI_COMPONENT` and an archetype that has it; it is not saved with
a scene yet, though the saved `"archetype"` names it. An engine component touches five places (the
`add-engine-component` skill); an enum shown in the editor needs `EnumTraits<T>` (`enum_traits.hpp`).

### Rendering

- `GraphicsContext` (`graphics_context.hpp`) owns the surface: device, `BeginFrame`, `Present`,
  resize. `Create(RenderingAPI)` returns null, stopping the application, for a backend not built.
- `Renderer` (`renderer.hpp`) has one method per pass plus target management; `DXRenderer`
  (`dx_*.cpp`) is the only one. `GetCapabilities()` (by default nothing) lets the editor hide
  controls that reach nothing.
- `RenderGraph` (`render_graph.hpp`) orders passes by named inputs and outputs and allocates the
  targets; `RenderingSystem::Start` builds it with `RenderGraphBuilder`. `AddResource` declares
  something ordered on but not allocated.

A disabled pass runs a bypass instead of leaving the graph, so nothing reads a stale image. After
the scene pass: resolve, `Ink`, `PlanarReflection`, `Particles`, volumetric fog, bloom, the editor's
outline, tone mapping, `Stylize`, `Overlay`. The cel look is a material's `toon` (banded by the
scene's `Stylization`), `Ink`'s outlines (from prepass depth, skinned meshes included) and
`Stylize`, which grades the picture and applies `ScreenEffects` -- a scene singleton a game writes
every frame for flashes, impact frames, speed lines and smears, never saved.

A new pass follows the `add-render-pass` skill. A new backend (Vulkan) is its own `GraphicsContext`
and `Renderer`, cases in `GraphicsContext::Create`, `RenderingSystem::SetRenderer` and `IsAvailable`
(`engine_config.cpp`), an ImGui backend in the editor and its `RendererCapabilities`; the HLSL
reaches it through dxc `-spirv`, given `SHADER_DEFINITIONS`.

Shaders are HLSL (Shader Model 6) in `engine/src/shader/`, embedded as strings and compiled at run
time, so editing one is a rebuild. Constants shared with C++ are `SHADER_DEFINITIONS`
(`shader_definitions.hpp`); the rest is in `engine/src/shader/CLAUDE.md`.

`Wind` (`wind.hpp`) is a scene singleton; a material's `MaterialFallback::sway` and `waves` say how
much of it a surface takes. Both are displaced in the vertex shader by `Displace`, kept word for
word in `scene.hlsl` and `shadow.hlsl` so shadows and the prepass (and so the ink) move too; the
depth passes set them as root constants per pool.

A `MeshAsset` whose `revision` rises is uploaded again and its ray-tracing structure dropped: that
cache is keyed by `DXMesh` address, and a stale entry reads freed memory.

### Procedural generation

Windowless and deterministic, so a map can be shared by its seed. `Random` (`random.hpp`) is a PCG32
written out because `<random>`'s distributions differ between standard libraries; `noise.hpp` hashes
its lattice. `IrregularGrid` (`irregular_grid.hpp`, grown by `Voronoi`) is a plane of convex cells
with point location, sides, and `Footprint` (the cells a shape covers at any angle).
`WaveFunctionCollapse` runs over any `WaveGraph` with `Restrict` and `Bias`, mending contradictions
by local repair. `NavMesh` is A* and a funnel over convex polygons with a cost each (`FromGrid`:
polygon `i` is cell `i`). `Flock` is boids; `MeshBuilder` puts flat-shaded meshes together.

### Assets and scenes

Assets are keyed by `AssetID` (a 128-bit UUID). One with no file goes in through
`Application::AddAsset` with a `MakeBuiltInAssetID`, so saved scenes still resolve it. A scene is
`<name>.json` plus a `<name>.assets.json` manifest; materials are `.mat` JSON; the editor's scenes
are in `editor/src/scene/`. An entity is saved with its `"archetype"` and loaded as it if the
application declares it (older files are guessed from their components); one undeclared is reported
and skipped.

`EngineConfig` (`kuki.config.json`) is installation-wide: backend, tone mapper, graphics settings.
`Preferences` are per application, loaded before `Start` and written after shutdown, rebound keys
included.

## Conventions

- C++23 without extensions: deducing this for const-correct accessors (`concepts.hpp`), trailing
  return types (`auto Foo() -> void`), `<bitset>` masks sized from the last enumerator. A constant is
  a `constexpr`, not a macro.
- Every engine macro is in `macros.hpp`. Third-party configuration (`NOMINMAX`,
  `GLM_ENABLE_EXPERIMENTAL`, ...) is `target_compile_definitions` in `engine/CMakeLists.txt`, and a
  single-header library's `..._IMPLEMENTATION` stays in its one source file.
- `.clang-format`: `ColumnLimit: 0` (no wrapping, one statement per line) and
  `MaxEmptyLinesToKeep: 0` (no blank lines in bodies). CMake: gersemi, 2-space indent.
- Comments explain *why*, at length, often with what was tried before; match them, and keep them
  when editing nearby.
- Commit messages: a lowercase imperative summary, then prose with the reasoning and measurements.
- Binary assets (`.png`, `.ttf`, `.fbx`, `.hdr`, `.exr`, ...) are Git LFS.
- CI (`build-and-test.yml`) builds `Debug` with tests and runs `ctest` (Windows/cl) on a push to any
  branch but `main`. `main` is protected, kept in step with the remote and never worked on: work
  reaches it by pull request once `cmake (windows-latest)` passes on the head. A fix gets a GitHub
  issue and a branch whose pull request closes it; other work needs no issue.
