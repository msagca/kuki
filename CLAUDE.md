# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

It holds what nearly every change needs. The rest is kept where it is used, so it is read only then:

- `editor/CLAUDE.md`, `game/duel/CLAUDE.md`, `game/frontier/CLAUDE.md`, `engine/src/shader/CLAUDE.md`
  are loaded when files in those folders are.
- Step-by-step recipes are skills in `.claude/skills/`: `capture` (build a target and look at what it
  draws), `add-engine-component`, `add-render-pass`, `add-compile-fail-check`.
- `.claude/settings.json` runs hooks: an edited C++ file has the lines that changed clang-formatted
  (only those -- some files have drifted from `.clang-format`, and reformatting a whole file would
  bury a change in noise), an edited CMake file is formatted with gersemi, and `external/` cannot be
  edited.

## Build, run, test

Submodules under `external/` are required: `git submodule update --init --recursive`.

All presets share one binary directory (`build/`), so switching presets reconfigures in place.

```bash
cmake --workflow Release          # configure + build + test + package
cmake --workflow Debug            # configure + build + test (CI runs these steps with tests on)
cmake --preset Debug && cmake --build --preset Debug   # iterate without packaging
```

Preset names: `Debug`, `Release` (default generator), `MSVC` with `MSVC-Debug`/`MSVC-Release` build
presets (Visual Studio), `Clang-Debug`/`Clang-Release` (Ninja + clang).

Presets set `KUKI_BUILD_GAMES=ON` and `KUKI_BUILD_TESTS=OFF`. Tests are opt-in, and cover the parts
with no window behind them -- the tick, gestures, the ECS, rollback and networking, the duel's rules.
`test/compile_fail/*.cpp` are the mistakes the engine promises to refuse at compile time, each passing
only if the compiler prints the message on its first line (the `add-compile-fail-check` skill).

```bash
cmake --preset Debug -DKUKI_BUILD_TESTS=ON && cmake --build --preset Debug
ctest --test-dir build --output-on-failure
ctest --test-dir build -R <TestName>      # single test; gtest_discover_tests registers one per case
```

Linting is opt-in too: `-DKUKI_ENABLE_LINTING=ON` attaches clang-tidy (bugprone/performance/
portability/clang-analyzer) to every target, header-filtered to each target's own `include/`.

Executables land in `build/editor/`, `build/game/<name>/`, `build/test/`; each target prints its
path at the end of its build. A game's folder gets fresh DLLs from its `<Target>Runtime` target, not
from building the game alone -- build both, or the game runs against the engine from before.

`--capture <file.png>` saves the frame drawn after `--capture-frame <n>` frames (300 by default) and
quits (Direct3D 12 only). It is how to see what a build draws without anyone at the screen (the
`capture` skill). Assets are staged next to the binary by post-build copies, and are resolved through
`ResolvePath` against the executable's directory — never the working directory.

On Windows the Direct3D 12 backend compiles HLSL at runtime through `dxcompiler.dll`/`dxil.dll`,
found in the Windows SDK at configure time and copied beside each executable. Without them no
shader compiles and nothing is drawn. Direct3D 12 is the only backend: OpenGL was removed, and a
machine or build without Direct3D 12 stops at startup saying so, rather than falling back.

## Layout

- `engine/` — `KukiEngine`, the library everything else links. Public headers in `include/`,
  included by bare filename (`#include <entity_manager.hpp>`).
- `editor/` — `KukiEditor`, a Dear ImGui application built on the engine. The only target that
  links imgui/imguizmo/file-browser.
- `game/` — one directory per game, discovered automatically: a subdirectory with a
  `CMakeLists.txt` is a game and needs no registration anywhere. A game links `KukiEngine` alone
  and must call `kuki_report_binary(${PROJECT_NAME})` itself.
- `test/` — gtest binary linking the engine, plus the duel's rules and the frontier's map compiled in.
- `external/` — submodules; never patched. Vendored-library quirks are worked around in the root
  `CMakeLists.txt`, which documents each one at length.

## Architecture

### Application lifecycle

`Application` (`engine/include/application.hpp`) is the facade: window, input, assets, scenes,
preferences, overlay. `Run()` is the only public lifecycle entry point; `Start`/`Update`/
`Shutdown`/`Status` are protected hooks a subclass overrides.

`SystemApplication<World, Systems...>` composes what a given application is made of as template
parameters: its `World` (every archetype its entities can be, see ECS below) and the systems it
runs, in order. Chess is `SystemApplication<ChessWorld, GameManager, RenderingSystem>`; the duel is
`SystemApplication<DuelWorld, AnimationSystem, NetworkSystem, DuelSystem, RenderingSystem>`.
`RenderingSystem` must be in the list (`static_assert`). Every system's `Start` runs before the
application's own `Start` (where a game loads assets and builds its scene), and every `PostStart`
after it -- a game's own system begins in `PostStart`, when its scene exists.

There are no scripts. Game behaviour is a game's own `System`, compiled into the game and listed in
its application: it owns the match's state, finds its entities with queries, and draws from
`Update`. An application is therefore a small subclass: see `game/chess/include/chess.hpp` and
`game/duel/include/duel.hpp`.

Command-line flags are declared, not scanned for: a system that reads any lists them as
`static constexpr std::array FLAGS` of `Flag` (`command_line.hpp`), and `SystemApplication::FLAGS`
joins every system's with the engine's and the editor's (`ENGINE_FLAGS`) and `static_assert`s that
no two share a name. Read them with `HasFlag`/`GetFlagValue` on `GetCommandLineArguments()`.
`--help` lists them all; an argument nothing declares is warned about at startup.

### Fixed tick and input

A frame runs two things. `System::Tick` is called a whole number of times -- none,
one or several -- each standing for `Application::TickTime()` seconds (sixty a second by default,
`TickClock` in `tick_clock.hpp`), and then `Update` is called once with the frame's own length.
Game rules go in `Tick`, which must leave the same state given the same state and the same buttons;
presentation goes in `Update`, which may blend by `TickAlpha()`.

A tick reads the player as one `ButtonMask` from `Application::GetButtons()`. A button is a named,
rebindable binding registered with `RegisterButton`, whose bit is its registration order; the mask
is latched per tick so a tap shorter than a tick still shows for one. `GestureRecognizer`
(`gesture.hpp`) turns a run of masks into presses, chords, holds, releases and sequences, counted
in ticks and written in a small notation (`"left right punch"`, `"guard*30 -guard"`). It keeps no
state but the masks, so one per player, fed the masks a peer sent, reproduces that peer's gestures.

An `Animator` with no `states` plays one clip by frame time, as the editor's preview does. With
states it is stepped by the tick (`StepAnimator`): a state plays a clip, fires named events on the
ticks they give, and goes to its `next` when a clip that does not loop ends; the game changes state
with `Play`. `rootNode` takes that node's travel out of the pose and reports it per tick in
`rootDelta`. The pose is sampled in `Update` between the last two ticks and crossfaded from whatever
was showing. List `AnimationSystem` before a game's own system for the game to see a tick's events
in that tick. `game/duel` uses all of it (`game/duel/CLAUDE.md`).

Inverse kinematics is presentation too: an animator's `ik` goals (`IKGoal`: `Reach` puts a bone on a
point by bending its parent and grandparent, `Aim` turns a bone and some parents to face one) are
solved in `AnimationSystem::LateUpdate` -- `System::LateUpdate` runs after every `Update` and before
`RenderingSystem` draws -- so they see where the game's systems put things this frame. Goals are solved from
the top of the skeleton down, the pose is restored before each solve (`IKHeld`), and a scene is
saved without the goals' turns. A goal's `target` entity is saved as its place in the file's entity
order and resolved after load. The maths is `SolveTwoBone`/`SolveAim` in `inverse_kinematics.hpp`.

The older `RegisterInputAction` sequences are a different thing: typed characters with no timing,
used for editor shortcuts.

### The settings menu, and no text that is a hint

Every game has `SettingsMenu` (`settings_menu.hpp`, owned by `Application`, off in the editor through
`ApplicationDescription::settingsMenu`): Escape -- the `Settings` binding, registered first -- opens
and closes it over the dimmed picture, in the overlay's text, hanging from the top of the window;
while it is open nothing else the overlay was given is drawn, and the arrow keys reach every line of
it, the row of pages included. Its pages are the game's own
(`GetSettingsMenu().AddChoice/AddToggle/AddAction(SettingsPage::Game, ...)`, each a get, a set and an
optional visible), the engine's graphics (saved in `EngineConfig`: display, vsync, anti-aliasing,
shadows, bloom, fog and clouds, reflections, a frame-rate counter), and the controls: every binding
in registration order, rebindable, saved in the game's `Preferences` as `input.<name>` and taken at
registration (`RegisterBinding`/`RegisterButton`). Quitting is its footer's; a game does not quit
itself on Escape. While it is open the game hears no keys, buttons, wheel or typed characters, what it
held is released (`InputManager::ReleaseAll`), and a game that is not online is `Paused` -- a clock
that runs in `Update` should check `GetPlayState`. `--settings <page>` starts with it open, for a
capture.

So a game puts no key hints on its scene: a key a game reads is a named binding with a description,
which is what the controls page lists, and an option a game offers belongs on its page, not drawn
over the picture. Mouse buttons are read directly; the menu cannot capture them.

### Games as libraries, and building scenes

A game is a library plus a launcher (`kuki_add_game` in the root `CMakeLists.txt`). Its sources hold
a `Setup<Game>(Application &)` that loads assets and builds the scene, `KUKI_GAME_MODULE(name,
setup)` and `KUKI_GAME_MAIN(AppClass)`; the executable is `cmake/game_main.cpp` for every game.
Under `BUILD_SHARED_LIBS` the library is `<Target>Game.dll` and the engine is `KukiEngine.dll`; a
static build compiles the sources into the executable. The editor no longer hosts games
(`editor/CLAUDE.md`).

`Application` has a `PlayState` (`Stopped`/`Playing`/`Paused`). Games are always `Playing`; only
the editor stops and pauses.

Scene content is built either through the engine calls (`CreateEntity<A>`, `GetEntityComponent`) or
the fluent builder in `game_builder.hpp`, which names each entity's archetype and checks every call
against it at compile time: `ChessWorld::Game(app, "Chess").Scene("Main").Entity<CameraObject>("Camera").ActiveCamera().Entity<MeshObject>("Pawn").Mesh("Cube")`.
`.Mesh()` on an entity of an archetype that is not drawn, or `.With<T>()` for a `T` its archetype lacks, does not
compile. `World::Game` also refuses an archetype the world does not declare; plain `kuki::Game(app)`
checks that only at run time.

### ECS

Archetype storage, with the archetypes fixed at compile time. An archetype is a type,
`Archetype<Transform, Renderable, Seat>` (`archetype.hpp`), whose parts are components or bundles of
them (`Components<...>`; the engine's are `Renderable`, `Animated` in `engine_archetypes.hpp`). An
archetype with a `Transform` is given a `WorldMatrix` beside it without naming it. An entity is made of one -- `CreateEntity<A>` -- and keeps it: **nothing
adds a component to an entity or removes one**. A component is a type with `ComponentTraits`: the
engine's are declared in `component_traits.hpp`, and a game declares its own with
`KUKI_COMPONENT(Type, index)`, taking ids from `FIRST_GAME_COMPONENT`. Using an undeclared type as a
component is a compile error.

A game's `World<As...>` (`world.hpp`) is the engine's archetypes (`EngineArchetypes`) plus its own.
It `static_assert`s that no two archetypes have the same components and no two components share an
id, and every scene of the application is given its archetypes when made (`SceneManager::Create`);
an `EntityManager` holds only archetypes it was told about (`Declare`), each a table of type-erased
columns made where the archetype's type is known, so a game's components are stored like the
engine's. Removing an entity swaps the last row of its table into the gap.

Queries go through `ForEach<Q...>`, which visits the archetypes whose signature has every required
component. `Optional<T>` reads a component where it exists without narrowing (a possibly-null
pointer); `Without<T>` skips archetypes that have it. A query over a non-component, or one requiring
nothing, does not compile. `World::ForEach<Q...>(app, f)` goes further: it refuses a query no
archetype of the world matches, and visits only the archetypes the compiler found to match
(`EntityManager::ForEachOf<A, Q...>`). `Delete` during a query waits for it to end; `Create` does
not, so a callback that creates in the archetype it is visiting must not use what it was handed after.

**Every drawn entity is in a pool** (`pool.hpp`). A drawn archetype has `Renderable`, which is the
tag `Pooled` and nothing else: the mesh, the material, the bounds and what a backend resolved them to
are its pool's, not the entity's. A pool is a `PoolKey` (a mesh, or a model's mesh by index, and a
material) and is a table of its own, so an archetype has its own table -- where an entity with no
mesh yet sits, undrawn -- and one more per pool; `ForEachOf` and `ForEach` visit them all. Make an
entity in one with `CreateEntity<A>(key)` (`World::Create<A>(app, key)`), give it another mesh or
material with `MoveEntityToPool` (the builder's `.Mesh()`/`.Material()` do), make one ahead with
`CreatePool` to give it bounds of its own (empty: never culled, and out of the shadow's and the probe
volume's fit). A table's `version` rises when a row comes, goes or moves, which is how a renderer
keeps what it worked out from a pool. The Direct3D 12 backend draws a pool as one instanced draw from its `WorldMatrix`
column, copied as it lies, once a frame for every pass (`ResolvePools`, `PreparePools`); it culls a
pool at a time and skinned meshes not at all.

A model is placed with `InstantiateAsset<Root>(name)`: the root is an entity of archetype `Root`
(`ModelRoot` unless the game wants its own components on it, as the duel's `Figure` does) standing for
the model's top node, with the nodes below as `ModelNodeObject`/`MeshObject` children, each mesh in
the pool of the model's mesh and material.

`Transform` carries no dirty bit, and no world matrix: that is the `WorldMatrix` component, written
by the transform update -- read where something is from it. After writing `position`/`rotation`/
`scale`, call `MarkTransformDirty` or the change is never applied. The transform update
is a linear pass over a preorder-flattened cache with packed dirty bits; the comments in
`entity_manager.hpp` explain the invariants that pass depends on.

Some components are scene singletons (`Component::IsSceneSingleton`) — `IndirectLighting`,
`AntiAliasing`, `AtmosphereSky` describe the scene, not the entity holding them.

**Adding a game's component** is a struct and a `KUKI_COMPONENT(Type, index)` line beside it, then
an archetype in the game's `World` that has it. Game components are not saved with a scene yet (the
serializer writes the engine's), though a saved entity's `"archetype"` names them.

**Adding an engine component** touches five places, in order; the `add-engine-component` skill lists
them. The column factory, the variants and the run-time `GetComponent(EntityID, ComponentType)` are
generated from the list. A new enum shown in the editor needs an `EnumTraits<T>` specialization in
`enum_traits.hpp`.

### Rendering

Three layers, each replaceable independently:

- `GraphicsContext` (`graphics_context.hpp`) owns the presentation surface: window hints, device,
  `BeginFrame`, `Present`, resize. `Create(RenderingAPI)` is the only place that names every
  backend, and returns null -- which stops the application -- for one this build has not got.
- `Renderer` (`renderer.hpp`) is the backend interface — one method per render pass plus target
  management. Implemented by `DXRenderer` (`dx_*.cpp`), the only backend.
  `GetCapabilities()` reports what a backend can actually do so the editor hides controls that
  reach nothing; it defaults to "none of it".
- `RenderGraph` (`render_graph.hpp`) declares passes with named inputs and outputs, resolves the
  edges, topologically flattens them, and allocates/resizes the targets. The graph, the same for
  every backend, is assembled in `RenderingSystem::Start` (`rendering_system.cpp`) with
  `RenderGraphBuilder`. `AddResource` declares something the graph orders on but does not allocate
  (the probe field's structured buffer).

A disabled pass still executes — it runs a bypass that leaves its output harmless — rather than
being dropped from the graph, which would leave consumers reading last frame's image.

The chain after the scene pass is: resolve, `Ink`, `PlanarReflection`, `Particles`, the volumetric
fog, the bloom chain, the editor's outline, tone mapping, `Stylize`, `Overlay`. The look of a cel is
three of those and the scene shader (`RendererCapabilities::stylization`): a material's `toon`
blends physical shading into the bands the scene's `Stylization` describes, `Ink` finds outlines
and folds in the prepass depth (which holds skinned meshes too), and `Stylize` grades the tone-mapped picture and applies the scene's `ScreenEffects` --
a scene singleton a game writes every frame for flashes, impact frames, speed lines and smears, and
which is never saved. `PlanarReflection` mirrors one level surface in screen space (wet ground,
puddles, rain rings). Particles have a box shape, a facing (camera, velocity streak, flat), a sprite
(soft, ring, petal), turbulence and an unsorted alpha blend, which is what the weather is made of.

**Adding a render pass** touches the enum, the renderer's interface and both its switches, the
backend and the graph; the `add-render-pass` skill lists them.

**Adding a backend** (Vulkan is the one named) is a `GraphicsContext` and a `Renderer` of its own,
a case in `GraphicsContext::Create`, `RenderingSystem::SetRenderer` and `IsAvailable`
(`engine_config.cpp`), its Dear ImGui backend beside Direct3D 12's in the editor, and its
`RendererCapabilities`. HLSL can reach it through dxc (`-spirv`), so the shaders need not be
rewritten; `SHADER_DEFINITIONS` must reach its compiler too.

### Shaders

HLSL (Shader Model 6), all under `engine/src/shader/`, embedded into the build as strings and
compiled at runtime -- so editing a shader is a rebuild, not a recompile of any C++ that uses it. Constants both sides must agree on live once in C++ (`SHADER_DEFINITIONS` in
`shader_definitions.hpp`). The rest is in `engine/src/shader/CLAUDE.md`.

### Procedural generation, navigation and flocking

Windowless and deterministic -- the same inputs make the same thing on every machine, which is what
lets two players share a map by its seed. `Random` (`random.hpp`) is a PCG32 written out because
`<random>`'s distributions differ between standard libraries; `GradientNoise`/`FractalNoise`/
`RidgedNoise` (`noise.hpp`) hash their lattice instead of using a table.

`IrregularGrid` (`irregular_grid.hpp`) is a plane cut into convex cells with however many neighbours
each one's shape gives it. `Voronoi` grows one from Poisson-disc points with Lloyd relaxation and
shrinks short sides to keep neighbour counts in `GridSettings`' range (4 to 8). The constructor takes
any polygons that share corners. It also does point location (a bucket grid), sides, corner-cells, and
`Footprint`: the cells a shape covers when put down at a point and turned to any angle, which is how
a freely rotated building of several cells finds its cells on a grid that has no "east".

`WaveFunctionCollapse` (`wave_function_collapse.hpp`) is the simple tiled model taken off the grid:
`WaveRules` say which tiles may be across a side (with optional side kinds for directional rules),
`WaveGraph` is any adjacency (`FromGrid`), `Restrict` fixes what a node may be, and `Bias` scales a
tile's weight per node, which is how a noise field shapes the result while the rules keep it
consistent. A contradiction is mended by local repair (unsettle round the failed node, rebuild,
widen if it fails again), not by backtracking; the header says why.

`NavMesh` (`nav_mesh.hpp`) is A* over convex polygons plus a funnel string-pull, with a cost and a
blocked flag per polygon; `FromGrid` makes polygon `i` cell `i`, so a map's movement layer is a cost
per cell. `Flock` (`flock.hpp`) is boids with a height band over a caller's ground. `MeshBuilder`
(`mesh_builder.hpp`) puts flat-shaded low-poly meshes together: faces, fans, frustums, boxes, gables,
rocks. `game/frontier` is built from all of it (`game/frontier/CLAUDE.md`).

A `MeshAsset` whose `revision` is raised is uploaded again, which also drops the mesh's ray-tracing
structure -- it is cached by `DXMesh` address, and a stale one reads freed memory.

### Wind

`Wind` (`wind.hpp`) is a scene singleton: a direction, a strength, gusts rolling along it, a flutter,
and waves. A material takes as much as its `MaterialFallback::sway` (bend per height above the
entity's origin squared) and `waves` (heave, shaded faceted) ask. Both are displaced in the vertex
shader by one `Displace` function kept word for word in `scene.hlsl` and `shadow.hlsl`, so shadows
and the prepass (and so the ink) move with the surface. Sway and waves ride in `DXInstanceData`
because the depth passes batch by mesh and get no material. Direct3D 12 only
(`RendererCapabilities::wind`).

### Assets and scenes

`AssetManager` keys assets by `AssetID` (a 128-bit UUID). Assets with no file behind them go in via
`Application::AddAsset`; give those a `MakeBuiltInAssetID` so a saved scene can still resolve them
across runs. Primitives (`Cube`, `Plane`, …) are loaded at startup.

Scenes serialize to JSON: `<name>.json` for entities and components, `<name>.assets.json` as a
sidecar manifest of the assets it references. Materials are standalone `.mat` JSON files.
Editor scenes live in `editor/src/scene/`. Each entity records its `"archetype"` (every component
name, saved or not), and loading makes it of that archetype if the application declares one; a file
from before guesses the archetype from the components it holds. An entity of an undeclared archetype
is reported and left out.

Two persistence layers that are easy to confuse: `EngineConfig` (`kuki.config.json`, installation-
wide — graphics backend, tone mapper, the settings menu's graphics) and `Preferences`
(per-application named values, loaded before `Start` and written after shutdown; rebound keys too).

## Conventions

- Every macro the engine defines is in `engine/include/macros.hpp`, which says what is
  deliberately kept elsewhere: configuration of third-party libraries (`GLM_ENABLE_EXPERIMENTAL`,
  `GLFW_INCLUDE_NONE`, `NOMINMAX`, `WIN32_LEAN_AND_MEAN`) is `target_compile_definitions` in
  `engine/CMakeLists.txt`, a single-header library's `..._IMPLEMENTATION` stays in its one source
  file, and what shaders are compiled with is `SHADER_DEFINITIONS`. A constant is a `constexpr`, not
  a macro.

- C++23 with no compiler extensions. The codebase leans on deducing this (`auto f(this auto &self)`
  for const-correct accessors, see `ConstCorrectPointer`/`ConstBasedValue` in `concepts.hpp`),
  trailing return types everywhere (`auto Foo() -> void`), and `<bitset>`-based masks sized from
  the last enumerator.
- `.clang-format` sets `ColumnLimit: 0` — lines are not wrapped, one statement per line, and
  `MaxEmptyLinesToKeep: 0` means no blank lines inside function bodies.
- CMake is formatted with gersemi (`.gersemirc`, 2-space indent).
- Comments in this codebase explain *why*, at length, and often record what was tried before. Match
  that register when touching commented code; do not strip these comments when editing nearby.
- Commit messages are lowercase imperative summaries with substantial prose bodies explaining the
  reasoning and measurements behind a change.
- Binary assets (`.png`, `.ttf`, `.fbx`, `.hdr`, `.exr`, …) are Git LFS.
- CI (`.github/workflows/build-and-test.yml`) configures the `Debug` preset with
  `KUKI_BUILD_TESTS=ON`, builds and runs `ctest` on a push to any branch but `main` (Windows/cl
  only for now). `main` is protected: work goes on a branch and reaches `main`
  by a pull request, which can merge once the `cmake (windows-latest)` check has passed on its head.
- Local `main` is kept in step with the remote's and never worked on. Fixing a problem: open a
  GitHub issue for it, fix it on a branch of its own, and merge that branch (its pull request closes
  the issue). Other work needs no issue and goes on a local branch of its own, which reaches `main`
  by a pull request like everything else.
