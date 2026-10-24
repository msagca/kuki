# Kuki

A hobby game engine project that I'm working on to learn about game engine architecture and graphics programming.

It is written in C++23 and is made up of:

- **KukiEngine**, the engine library: an archetype-based ECS fixed at compile time, a fixed-tick game loop with rebindable buttons and a gesture recognizer, skeletal animation with state machines and inverse kinematics, rollback networking, and a render graph with Direct3D 12 and OpenGL backends (physically based lighting, image-based lighting, shadows, bloom, a computed sky with clouds and fog, anti-aliasing).
- **KukiEditor**, a [Dear ImGui](https://github.com/ocornut/imgui) editor for building, saving and playing scenes.
- **Games** built on the engine, in `game/`:
  - `chess`, two players at one board with a chess clock, moving pieces with the mouse.
  - `duel`, a small one-on-one fighting game, played on one machine or between two over the network.

## Installation

### Prerequisites

- [CMake](https://cmake.org) 3.31 or newer
- [Git](https://git-scm.com) with [Git LFS](https://git-lfs.com) (binary assets such as models, textures and fonts are stored in LFS)
- A C++23 compiler: Visual Studio 2022, Clang 18+ or GCC 14+
- [Blender](https://www.blender.org) (optional), which exports the games' `.blend` models during the build; without it the games build, with a warning, but have no models

#### Windows

- [Visual Studio](https://visualstudio.microsoft.com) 2022 (with the C++ workload and a Windows SDK)

The Direct3D 12 backend compiles its shaders at runtime with `dxcompiler.dll` and `dxil.dll`, which are found in the Windows SDK and copied next to each executable. If they are missing, applications fall back to OpenGL.

#### Linux

- [Clang](https://llvm.org) or [GCC](https://gcc.gnu.org)
- [Mesa 3D](https://mesa3d.org)
- [Ninja](https://ninja-build.org)
- [X.Org](https://www.x.org)

On [Debian](https://www.debian.org)-based systems (e.g., [Ubuntu](https://ubuntu.com)), you can install the prerequisites using the following commands:

```bash
sudo apt update
```

```bash
sudo apt install -y build-essential clang cmake git git-lfs libgl1-mesa-dev ninja-build xorg-dev
```

> Your distribution's CMake may be older than 3.31; if so, install a newer one from [cmake.org](https://cmake.org/download/) or with `pip install cmake`.

Linux builds use OpenGL. Continuous integration only builds on Windows at the moment, so Linux builds may break without notice.

### Build Instructions

- Clone the repository with submodules

```bash
git lfs install
git clone --recursive https://github.com/msagca/kuki
```

> If you have already cloned it without `--recursive`, run `git submodule update --init --recursive` to fetch the submodules. If you cloned it before installing Git LFS, run `git lfs pull` to fetch the assets.

- Navigate to the project directory

```bash
cd kuki
```

- Configure and build the project

```bash
cmake --workflow Release
```

> If it fails due to missing dependencies, refer to the error messages to install the required packages. Then, run the command again.

If it succeeds, you can launch `KukiEditor` from `build/editor/` and the games from `build/game/<name>/`.

> Refer to the build output for the exact path of each executable.

### Presets

All presets share the `build/` directory, so switching between them reconfigures in place.

| Preset | Generator |
| --- | --- |
| `Debug`, `Release` | CMake's default |
| `MSVC` (build presets `MSVC-Debug`, `MSVC-Release`) | Visual Studio |
| `Clang-Debug`, `Clang-Release` | Ninja with Clang |

`cmake --workflow Debug` configures, builds and tests a debug build. To iterate without packaging, use `cmake --preset Debug && cmake --build --preset Debug`.

### Tests and linting

Tests and linting are off by default.

```bash
cmake --preset Debug -DKUKI_BUILD_TESTS=ON
cmake --build --preset Debug
ctest --test-dir build --output-on-failure
```

Pass `-DKUKI_ENABLE_LINTING=ON` at configure time to run clang-tidy on every target.

## Playing the games

Every application lists its command-line flags with `--help`.

- **Chess**: click a piece to select it, then a green square to move it (red marks a piece it can take); drag anywhere else to turn the board; `R` restarts the game and `Esc` quits.
- **Duel**: `A`/`D` step left and right, `J` punches, `K` kicks and `Esc` quits. Run one instance with `--host <port>` and another with `--join <host:port>` to play over the network; `--lag`, `--jitter` and `--loss` simulate a bad connection on a single machine.

## License

[MIT](LICENSE)
