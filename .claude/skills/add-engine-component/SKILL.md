---
name: add-engine-component
description: Checklist for adding a component to the Kuki engine itself (not a game's own component) -- the enum, the traits list, the display name, an archetype, serialization and the editor. Use whenever a new engine component type is introduced.
---

# Add an engine component

A game's own component is only a struct, a `KUKI_COMPONENT(Type, index)` line and an archetype in
its `World` -- this skill is for the engine's. The column factory, the variants and the run-time
`GetComponent(EntityID, ComponentType)` are generated from the list, so these are the only places:

1. `engine/include/component_type.hpp` -- the `ComponentType` enumerator. Order matters: everything
   below follows it.
2. `engine/include/component_traits.hpp` -- a `KUKI_ENGINE_COMPONENT` line, and the type's place in
   `EngineComponents`, both in the enum's order. `static_assert`s check both, so a mismatch is a
   compile error, not a silent misindex.
3. `engine/include/component.hpp` -- the header's include. `engine/src/component.cpp` -- the display
   name, and `IsSceneSingleton` if it describes the scene rather than the entity holding it (as
   `IndirectLighting`, `AtmosphereSky`, `Wind` do).
4. An archetype that has it: `engine/include/engine_archetypes.hpp` (and `EngineArchetypes`), or a
   game's `World`. Nothing adds a component to an existing entity, so without an archetype the
   component exists on nothing.
5. If it should persist, `engine/src/scene_serializer.cpp`: `SERIALIZED_TYPES`, a `ToJson`/`FromJson`
   pair, a `ToJsonVisitor::operator()` overload, and the load switch. If it should not (derived or
   written every frame, like `ScreenEffects`), say why in a comment beside the struct.

Then the editor:

- `editor/include/property_displayer.hpp` -- how its fields are shown and edited.
- `Editor::IsSupported` (`editor/src/editor.cpp`) -- a case returning the `RendererCapabilities`
  flag it needs, if only one backend draws it.
- A new enum among its fields needs an `EnumTraits<T>` specialization in `enum_traits.hpp`.

Finally build with tests on and run them (`ctest --test-dir build --output-on-failure`); the ECS and
archetype tests exercise the generated column factory.
