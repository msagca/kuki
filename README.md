# Kuki

A hobby game engine project that I'm working on to learn about game engine architecture and graphics programming.

## Installation

### Prerequisites

- [CMake](https://cmake.org)
- [Git](https://git-scm.com)

#### Windows

- [Visual Studio](https://visualstudio.microsoft.com) (with C++ workload)

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
sudo apt install -y build-essential clang cmake git libgl1-mesa-dev ninja-build xorg-dev
```

### Build Instructions

- Clone the repository with submodules

```bash
git clone --recursive https://github.com/msagca/kuki
```

> If you have already cloned it without `--recursive`, run `git submodule update --init --recursive` to fetch the submodules.

- Navigate to the project directory

```bash
cd kuki
```

- Configure and build the project

```bash
cmake --workflow --preset Release
```

> If it fails due to missing dependencies, refer to the error messages to install the required packages. Then, run the command again.

If it succeeds, you can launch `KukiEditor` from the build directory.

> Refer to the build output for the exact path of the executable.
