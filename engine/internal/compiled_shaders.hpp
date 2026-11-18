#pragma once
#include <kuki_engine_export.h>
#include <span>
#include <string_view>
namespace kuki {
/// @brief The DXIL of one entry point of the engine's shaders, compiled when the engine was built.
///
/// `KukiShaderCompiler` (`engine/tools/compile_shaders.cpp`) compiles every entry point of every
/// file in `engine/src/shader/` with `SHADER_DEFINITIONS` and writes the bytecode into a source file
/// the engine is built with, which defines this. Nothing compiles HLSL at run time.
///
/// @param file The shader file's stem: `scene` for `scene.hlsl`.
/// @param entryPoint The function, named for its stage: `VSMain`, `PSOcclusionBlur`, `CSTrace`.
/// @return The signed bytecode, or an empty span when the file has no such entry point -- a name the
///   C++ asks for and no shader defines, since one that does not compile fails the build instead.
KUKI_ENGINE_API auto FindCompiledShader(std::string_view file, std::string_view entryPoint) -> std::span<const unsigned char>;
} // namespace kuki
