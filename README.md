# Kuki

A hobby game engine project that I'm working on to learn about game engine architecture and graphics programming.

## Installation

### Prerequisites

- [CMake](https://cmake.org)
- [Git](https://git-scm.com)

#### Windows

- [Visual Studio](https://visualstudio.microsoft.com/vs/community) (with C++ workload)

#### Linux

- [GCC](https://gcc.gnu.org) or [Clang](https://llvm.org)
- [Mesa 3D](https://mesa3d.org)
- [Ninja](https://ninja-build.org)
- [X.Org](https://www.x.org)

On Debian-based systems (e.g., [Ubuntu](https://ubuntu.com)), you can install the prerequisites using the following commands:

```bash
sudo apt update
```

```bash
sudo apt install -y build-essential cmake git libgl1-mesa-dev ninja-build xorg-dev
```

### Build Instructions

- Clone the repository with submodules

```bash
git clone --recursive https://github.com/msagca/kuki
```

> If you have already cloned the repository without `--recursive`, run `git submodule update --init --recursive` to fetch the submodules.

- Navigate to the project directory

```bash
cd kuki
```

- Configure the project with CMake

```bash
cmake -B build --preset Release
```

> If this step fails due to missing dependencies, refer to the error messages to install the required packages.

- Build the project

```bash
cmake --build build --preset Release
```

- Install the binaries (optional)

```bash
cmake --install build
```

> Install command may require administrative privileges to write to system directories.

- Run the editor application

> Refer to the build/install output for the exact path of the executable.
