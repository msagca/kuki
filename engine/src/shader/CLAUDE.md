# Shaders

HLSL, Shader Model 6, for Direct3D 12, with entry points named per stage (`VSMain`, `PSMain`,
`CSTrace`). A later backend can take them through dxc (SPIR-V for Vulkan).

`cmake/EmbedShaders.cmake` embeds each file into the generated `embedded_shaders.hpp` as a
`std::string_view` named after it (`scene_hlsl`), and dxc compiles it at run time. Editing a shader
is a rebuild, and one that does not compile shows only when run: read the log or `--capture` a frame.

Constants shared with C++ come in as definitions from `SHADER_DEFINITIONS` (`shader_definitions.hpp`);
a shader not given one fails to compile rather than using a stale copy.

Shader files are `HEADER_FILE_ONLY` with `VS_TOOL_OVERRIDE "None"`, or MSBuild hands `.hlsl` to FXC
and the build fails.

`Displace` (wind sway and waves) is kept word for word in `scene.hlsl` and `shadow.hlsl`: change
both, or shadows and the prepass (and so the ink) stop moving with the surface.
