---
name: add-engine-component
description: Checklist for adding a component to the Kuki engine itself (not a game's own component) -- the enum, the traits list, the display name and an archetype. Use whenever a new engine component type is introduced.
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

A new enum among its fields that the settings menu lists by name needs an `EnumTraits<T>`
specialization in `enum_traits.hpp`. If only one backend draws it, the entries that set it are
hidden behind a `RendererCapabilities` flag.

Finally build with tests on and run them (`ctest --test-dir build --output-on-failure`); the ECS and
archetype tests exercise the generated column factory.
