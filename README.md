# Learning OpenGL

This is where I experiment with OpenGL as I follow along the tutorials at [LearnOpenGL](https://learnopengl.com/).

## Building

### Requirements

- *Windows:* [Visual Studio](https://visualstudio.microsoft.com/vs/community/), `git`
- *Linux:* `cmake`, `ninja`, `gcc`, `g++`, `git`

### Instructions

- Install [vcpkg](https://github.com/microsoft/vcpkg)

- Set the environment variable `VCPKG_ROOT` and append it to `PATH`

- Clone this repository

```bash
git clone https://github.com/msagca/learning-opengl
```

- Run the configure command

```bash
cmake .
```

> :warning: You may have to specify a preset suitable for your OS

```bash
cmake --preset linux-debug .
```

- Run the build command

```bash
cmake --build .
```
