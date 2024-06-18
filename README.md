# Learning OpenGL

This is where I experiment with OpenGL as I follow along the tutorials at [LearnOpenGL](https://learnopengl.com/).

## How to Build

### Requirements

- `git`, [vcpkg](https://github.com/microsoft/vcpkg)
- **Windows:** [Visual Studio](https://visualstudio.microsoft.com/vs/community/) (with CMake)
- **Linux:** `cmake`, `ninja`, `gcc`, `g++`

### Instructions

- Set the environment variable `VCPKG_ROOT` and append it to `PATH`

- Clone this repository

```bash
git clone https://github.com/msagca/learning-opengl
```

- Run the configure command

```bash
cmake .
```

> You may have to specify a preset suitable for your OS

```bash
cmake --preset linux-debug .
```

- Run the build command

```bash
cmake --build .
```
