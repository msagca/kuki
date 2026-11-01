# The frontier

A strategy map with no units yet, all made in code. `GenerateMap` (`terrain.cpp`, windowless and
tested) grows a grid, collapses nine terrains over it biased by an island field and ridges rising from
the shore, raises terraces and peaks, lays resource sites (stone and ore over two or three cells), and
builds the movement layer and navmesh. `MapSystem` builds the scene (a mesh per terrain, step faces,
the sea in three tints, shore foam, props as entities) and draws the layers, the map and hovered-cell
panels (the settings' "Map details") and a debug path (two right clicks). `PlacementSystem` places
`props.cpp`'s buildings, freely rotated, through `Footprint`; `FlockSystem` flies gulls and swallows;
`AmbienceSystem` moves the wind, clouds, emitters, foam and the sun (the day passes on a binding or
setting). Every key is a binding in `controls.hpp`, registered in order by `RegisterControls`.

The sea is opaque on purpose: `PlanarReflection` reads the depth prepass, which has no see-through
surfaces.

`--seed`, `--layer` (cells, resources, movement, building), `--distance` (6 to 70) and `--demo` (a
path and a few buildings) set up what a `--capture` shows.
