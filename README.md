# Learning OpenGL

This is where I experiment with OpenGL as I follow along the tutorials at [LearnOpenGL](https://learnopengl.com/).

## How to Build

### Requirements

- `git`, `cmake`, [vcpkg](https://github.com/microsoft/vcpkg)
- **Windows:** [Visual Studio](https://visualstudio.microsoft.com/vs/community/)
- **Linux:** `ninja` (or `make`), `gcc`, `g++`

### Instructions

- Set up `vcpkg` (follow steps *1..2* [here](https://learn.microsoft.com/en-us/vcpkg/get_started/get-started))

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
