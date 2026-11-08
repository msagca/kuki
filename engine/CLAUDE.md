# The engine

What the root `CLAUDE.md` leaves out because only work inside the engine, or deep use of one of its
parts, needs it. The rules, the ECS and what a game calls are there; shaders are in
`src/shader/CLAUDE.md`.

## Animation

An `Animator` is plain data. Its states are an `AnimatorController` asset it names by id, shared by
every animator on the same moves; without one it plays one clip by frame time (the editor's
preview). With states it is stepped by the tick (`StepAnimator`): a state plays a clip, fires
events (`fired`, a mask over the state's events, read with `Fired`), and goes to its `next` when a
clip that does not loop ends; the game changes state with `Play(animator, controller, name)`. The
controller's `rootNode` travel is taken out of the pose and reported as `rootDelta`. The pose is
sampled in `Update` into the model's `Rig` -- its nodes' entities, pose, fade and IK hold, kept by
the scene in a `SlotStore` (`Scene::GetRig`, swept each frame) -- and crossfaded. IK goals (up to
`MAX_IK_GOALS`; `Reach`, `Aim`; `inverse_kinematics.hpp`) are solved in
`AnimationSystem::LateUpdate`, top of the skeleton down, from the restored pose (`IKHeld`), and are
never saved into it; a goal's `target` is saved as an entity index and resolved after load.

## Rendering

- `GraphicsContext` (`graphics_context.hpp`) owns the surface: device, `BeginFrame`, `Present`,
  resize. `Create(RenderingAPI)` returns null, stopping the application, for a backend not built.
- `Renderer` (`renderer.hpp`) has one method per pass plus target management; `DXRenderer`
  (`dx_*.cpp`) is the only one. `GetCapabilities()` (by default nothing) lets the editor hide
  controls that reach nothing.
- `RenderGraph` (`render_graph.hpp`) orders passes by named inputs and outputs and allocates the
  targets; `RenderingSystem::Start` builds it with `RenderGraphBuilder`. `AddResource` declares
  something ordered on but not allocated.

A disabled pass runs a bypass instead of leaving the graph, so nothing reads a stale image. Before
the scene pass, `AmbientOcclusion` finds the creases in the depth prepass, and the scene pass
scales its indirect light by it. After the scene pass: resolve, `Ink`, `PlanarReflection`,
`Particles`, volumetric fog, `DepthOfField`, bloom, the editor's outline, tone mapping, `Stylize`,
`Overlay`. The cel look is a material's `toon` (banded by the
scene's `Stylization`), `Ink`'s outlines (from prepass depth, skinned meshes included) and
`Stylize`, which grades the picture and applies `ScreenEffects`.

Direct3D 12 draws a pool as one instanced draw from its `WorldMatrix` column and culls per pool;
skinned meshes are not culled.

A new pass follows the `add-render-pass` skill, and a new graphics setting the
`add-graphics-setting` skill. A new backend (Vulkan) is its own `GraphicsContext` and `Renderer`,
cases in `GraphicsContext::Create`, `RenderingSystem::SetRenderer` and `IsAvailable`
(`engine_config.cpp`), an ImGui backend in the editor and its `RendererCapabilities`; the HLSL
reaches it through dxc `-spirv`, given `SHADER_DEFINITIONS`.

Shaders are HLSL (Shader Model 6) in `src/shader/`, embedded as strings and compiled at run time,
so editing one is a rebuild. Constants shared with C++ are `SHADER_DEFINITIONS`
(`shader_definitions.hpp`).

`Wind` (`wind.hpp`) is a scene singleton; a material's `MaterialFallback::sway` and `waves` say how
much of it a surface takes. Both are displaced in the vertex shader by `Displace`, kept word for
word in `scene.hlsl` and `shadow.hlsl` so shadows and the prepass (and so the ink) move too; the
depth passes set them as root constants per pool.

A `MeshAsset` whose `revision` rises is uploaded again and its ray-tracing structure dropped: that
cache is keyed by `DXMesh` address, and a stale entry reads freed memory.

## Procedural generation

`Random` (`random.hpp`) is a PCG32 written out because `<random>`'s distributions differ between
standard libraries; `noise.hpp` hashes its lattice. `IrregularGrid` (`irregular_grid.hpp`, grown by
`Voronoi`) is a plane of convex cells with point location, sides, and `Footprint` (the cells a shape
covers at any angle). `WaveFunctionCollapse` runs over any `WaveGraph` with `Restrict` and `Bias`,
mending contradictions by local repair. `NavMesh` is A* and a funnel over convex polygons with a
cost each (`FromGrid`: polygon `i` is cell `i`). `Flock` is boids; `MeshBuilder` puts flat-shaded
meshes together.

## Scene files

A scene is `<name>.json` plus a `<name>.assets.json` manifest; materials are `.mat` JSON; animator
controllers are written once into the scene's `"controllers"` (an animator from before them,
holding its own states, gets a controller of its own on load); the editor's scenes are in
`editor/src/scene/`. An entity is saved with its `"archetype"` and loaded as it if the application
declares it (older files are guessed from their components); one undeclared is reported and
skipped.
