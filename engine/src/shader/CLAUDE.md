# Shaders

HLSL, Shader Model 6, for Direct3D 12, with entry points named per stage (`VSMain`, `PSMain`,
`CSTrace`). A later backend can take them through dxc (SPIR-V for Vulkan).

`KukiShaderCompiler` (`engine/tools/compile_shaders.cpp`) compiles every entry point of every file
here while the engine is built, and writes the DXIL into a generated source the engine is built
with, where `FindCompiledShader("scene", "PSMain")` finds it. A shader that does not compile fails
the build with dxc's message. An entry point is any function at the start of a line named for its
stage, `VS`/`PS`/`CS` and a capital, so nothing lists them and nothing else may be named so. A file
that uses `RayQuery` is compiled for Shader Model 6.5, the rest for 6.0. Debug compiles without
optimisation and puts the debug information in PDBs beside the generated source in the build
tree, for PIX.

Constants shared with C++ come in as definitions from `SHADER_DEFINITIONS` (`shader_definitions.hpp`);
a shader not given one fails to compile rather than using a stale copy.

Shader files are `HEADER_FILE_ONLY` with `VS_TOOL_OVERRIDE "None"`, or MSBuild hands `.hlsl` to FXC
and the build fails.

`Displace` (wind sway and waves) is kept word for word in `scene.hlsl` and `shadow.hlsl`: change
both, or shadows and the prepass (and so the ink) stop moving with the surface.
