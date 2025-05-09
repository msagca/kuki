# Kuki

I'm teaching myself graphics programming, game engine architecture and C++ through this hobby project.

## How to Build

### Requirements

- `git`, `cmake`
- **Windows:** [Visual Studio](https://visualstudio.microsoft.com/vs/community) (install **Desktop development with C++** workload)
- **Linux:** `ninja` (or `make`), `gcc`, `g++`

### Instructions

- Clone this repository including its submodules

```bash
git clone --recursive https://github.com/msagca/kuki
```

```bash
cd kuki
```

- Run the configure command

```bash
cmake -B build
```

- Run the build command

```bash
cmake --build build
```
