# Kuki

A game engine written in C++23, made up of:

- **KukiEngine**, the engine library: an archetype-based ECS fixed at compile time, a fixed-tick game loop with rebindable buttons and a gesture recognizer, skeletal animation with state machines and inverse kinematics, rollback networking, procedural generation (irregular grids, wave function collapse over any graph, noise), navigation meshes, flocking, and a render graph with a Direct3D 12 backend (physically based lighting, image-based lighting, shadows, bloom, a computed sky with clouds and fog, anti-aliasing).
- **Sample games**: chess, a networked one-on-one fight and a procedurally generated strategy map, each built entirely in code.
- **kuki**, the command a game is started and looked after with, in a repository of its own with the engine as a submodule.

## Documentation

| Read | For |
| --- | --- |
| [cli/](cli/README.md) | Starting a game with the `kuki` command |
| [docs/building/](docs/building/README.md) | Building the engine: prerequisites, presets, tests and linting |
| [template/](template/README.md) | The template a new game starts from |

## License

[MIT](LICENSE)
