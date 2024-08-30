# Learning OpenGL

This is where I experiment with `OpenGL` as I follow along the tutorials at [LearnOpenGL](https://learnopengl.com).

## How to Build

### Requirements

- `git`, `cmake`, [vcpkg](https://github.com/microsoft/vcpkg)
- **Windows:** [Visual Studio](https://visualstudio.microsoft.com/vs/community)
- **Linux:** `ninja` (or `make`), `gcc`, `g++`

### Instructions

- Set up `vcpkg` (follow steps 1..2 [here](https://learn.microsoft.com/en-us/vcpkg/get_started/get-started))

- Clone this repository

```bash
git clone https://github.com/msagca/learning-opengl
```

- Run the configure command

```bash
cmake .
```

> If `CMake` uses `Visual Studio` as the generator (default on `Windows`) but you use a different code editor (e.g., [Neovim](https://neovim.io)), header files installed via `vcpkg` might not be found by the [LSP](https://en.wikipedia.org/wiki/Language_Server_Protocol) (e.g., `clangd`). To fix this, you have to specify `Ninja` or `Unix Makefiles` as the generator so that the required `compile_commands.json` file is created.
>
> ```bash
> cmake -G "Ninja" .
> ```

- Run the build command

```bash
cmake --build .
```
