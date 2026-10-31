# Shaders

HLSL (`.hlsl`, Shader Model 6, entry points named per stage: `VSMain`, `PSMain`, `CSTrace`) for
Direct3D 12, the only backend. The GLSL went with the OpenGL backend; a later backend that can take
HLSL through dxc (SPIR-V for Vulkan) reuses these.

Nothing compiles ahead of time. `cmake/EmbedShaders.cmake` embeds every shader file into a
generated `embedded_shaders.hpp` as `std::string_view` constants named after the file
(`skybox_hlsl`, `scene_hlsl`), and dxc compiles them at runtime. Editing a shader is therefore a
rebuild, not a recompile of any C++ that uses it -- and a shader that does not compile shows only
when it is run, so look at the log or `--capture` a frame (the `capture` skill).

Constants the C++ and the shaders must agree on live once in C++ and are passed as preprocessor
definitions — see `SHADER_DEFINITIONS` in `shader_definitions.hpp`. A shader reading such a macro
without being given it fails to compile rather than falling back on a stale copy.

Shader files are marked `HEADER_FILE_ONLY` with `VS_TOOL_OVERRIDE "None"`, which is load-bearing:
MSBuild otherwise hands `.hlsl` to FXC and fails the build.

`Displace` (wind sway and waves) is kept word for word in `scene.hlsl` and `shadow.hlsl`: change
both, or shadows and the prepass (and so the ink) stop moving with the surface.
