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

- Update the submodules if necessary

```bash
git submodule update --init --recursive
```

- Create a build directory

```bash
mkdir build
```

```bash
cd build
```

- Run the configure command

```bash
cmake ..
```

- Run the build command

```bash
cmake --build .
```
