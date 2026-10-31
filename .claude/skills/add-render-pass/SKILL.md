---
name: add-render-pass
description: Checklist for adding a render pass to Kuki's render graph -- the enum and its name, the Renderer interface, the dispatch and bypass switches, the backend, the graph wiring, and a capability flag. Use whenever a new pass is introduced.
---

# Add a render pass

1. `engine/include/render_pass.hpp` -- the `RenderPass` enumerator. `RENDER_PASS_COUNT` derives
   from the last one, so add it before that if it is not meant to be last.
2. `engine/include/enum_traits.hpp` -- its name in `EnumTraits<RenderPass>`.
3. `engine/include/renderer.hpp` -- a pure virtual on `Renderer`.
4. `engine/src/renderer.cpp` -- the dispatch switch **and** the `BypassPass` switch. A disabled pass
   still runs its bypass rather than leaving the graph, or whatever reads its output reads last
   frame's image.
5. Each backend: `DXRenderer` (`dx_*.cpp`) is the only one now. A backend with no such path
   implements it as `BypassCopy(inputs, outputs)`, which is the honest one-liner.
6. `RenderingSystem::Start` (`engine/src/rendering_system.cpp`) -- a `BeginPass`/`AddInput`/
   `AddOutput` block, placed in the chain where it belongs (the root `CLAUDE.md` lists the chain
   after the scene pass).
7. If only one backend implements it, a flag in `RendererCapabilities`, set by that backend's
   `GetCapabilities()`, and a case in `Editor::IsSupported` for the components that drive it, so the
   editor hides controls that would reach nothing.

Shaders go in `engine/src/shader/` and are embedded at build time; any constant the C++ side also
needs goes in `SHADER_DEFINITIONS` (`shader_definitions.hpp`).

Then build and look at it with the `capture` skill -- a pass that compiles can still draw nothing.
