# Learning OpenGL

This is where I experiment with OpenGL as I follow along the tutorials at [LearnOpenGL](https://learnopengl.com/).

## How to Build

### Requirements

- `git`, [vcpkg](https://github.com/microsoft/vcpkg)
- **Windows:** [Visual Studio](https://visualstudio.microsoft.com/vs/community/)
- **Linux:** `cmake`, `ninja`, `gcc`, `g++`

### Instructions

- Set the environment variable `VCPKG_ROOT` and append it to the `PATH`

- Clone this repository

```powershell
git clone https://github.com/msagca/learning-opengl
```

- Run the configure command

```powershell
cmake .
```

- Run the build command

```powershell
cmake --build .
```
