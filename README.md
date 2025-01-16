# Learning OpenGL

This is where I experiment with `OpenGL`.

## How to Build

### Requirements

- `git`, `cmake`
- **Windows:** [Visual Studio](https://visualstudio.microsoft.com/vs/community)
- **Linux:** `ninja` (or `make`), `gcc`, `g++`

### Instructions

- Clone this repository with all its submodules

```bash
git clone --recursive https://github.com/msagca/learning-opengl
```

```bash
cd learning-opengl
```

- Run the configure command

```bash
cmake -B build .
```

- Run the build command

```bash
cmake --build build
```

- Before running the program, you need to download [this material](https://www.fab.com/listings/bb875143-5df0-42be-a8f7-5937b25660e4) (1K resolution) and copy the contents of the textures folder to `source/image` folder since this is the default material used for rendering
