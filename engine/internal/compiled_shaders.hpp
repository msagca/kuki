#pragma once
#include <cstdint>
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
/// @brief A hash of every entry point's name and bytecode, the same for the same shaders on every
/// machine. What the pipeline library on disk is stamped with (`DXPipelineCache`): a library made
/// from other shaders is thrown away rather than searched.
KUKI_ENGINE_API auto CompiledShadersDigest() -> uint64_t;
} // namespace kuki
