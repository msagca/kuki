# The frontier

A strategy map with no units yet, and everything in it is made in code, from the engine's procedural
generation (root `CLAUDE.md`). `GenerateMap` (`terrain.cpp`, windowless and tested) grows a grid,
collapses nine terrains over it biased by two ranked fields (an island for where the sea is, ridges
rising from the shore for the land), raises terraces and peaks, lays resource sites over whole cells
(stone and ore over two or three), and builds the movement layer and navmesh. `MapSystem` builds the
scene from it (a mesh a terrain, step faces, the sea in three tints, shore foam, props as entities),
draws the layers, the map's and the hovered cell's panels (the settings' "Map details"), and a debug
path (two right clicks). Every key is a binding named in `controls.hpp` and registered together by
`RegisterControls`, so the controls page lists them in order; nothing on the scene names a key.
`PlacementSystem` places buildings from `props.cpp`'s catalogue with free rotation through
`Footprint`; `FlockSystem` flies gulls and swallows; `AmbienceSystem` moves the wind, the clouds,
every wandering emitter, the foam and the sun (the day passes on a binding, or the settings).

The sea is opaque on purpose: `PlanarReflection` reads the depth prepass, which has no see-through
surfaces.

`--seed`, `--layer` (cells, resources, movement, building), `--distance` (6 to 70) and `--demo` (a
path and a few buildings) set up what a `--capture` shows.
