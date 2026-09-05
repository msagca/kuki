#pragma once
#include <cstdint>
#include <light_limits.hpp>
namespace kuki {
/// @brief One preprocessor definition handed to a shader at compile time.
struct ShaderDefinition {
  const char *name;
  uint32_t value;
};
/// @brief Values the C++ decides and every shader is compiled with, in either language.
///
/// A constant that both the engine and its shaders have to agree on has nowhere good to live. Put
/// it in the shader and the C++ sizing the buffer is guessing; put it in the C++ and the shader is.
/// Writing it in both is what the codebase did, and the failure is silent in both directions -- see
/// the note on `MAX_POINT_LIGHTS`.
///
/// Neither backend compiles its shaders ahead of time, and that is what makes this fixable. The
/// HLSL goes through `dxcompiler.dll` at runtime and the GLSL through the driver, so both take
/// their source as a string and both can be handed definitions alongside it. The shader declares
/// nothing and reads the macro; the C++ constant above is the only place the number appears.
///
/// A shader that reads one of these without being given it does not fall back on a stale copy. It
/// fails to compile, with the compiler naming the macro it could not resolve, which is what makes
/// this safe to rely on rather than merely tidy.
///
/// Prefixed because these land in every shader in the engine, including the ones with no interest
/// in them, and a macro that broad should be obvious about where it came from.
///
/// Each arrives with an unsigned suffix on it. Every value here is a count, both languages type a
/// count as unsigned, and GLSL will not compare a `uint` against a signed literal -- `min(count,
/// KUKI_MAX_POINT_LIGHTS)` is an error rather than a promotion if the macro expands to a bare
/// number. HLSL is happy either way, and is given the same form so the two compile paths differ in
/// nothing but the flag that carries them.
inline constexpr ShaderDefinition SHADER_DEFINITIONS[]{
  {"KUKI_MAX_POINT_LIGHTS", MAX_POINT_LIGHTS},
  {"KUKI_MAX_SPOT_LIGHTS", MAX_SPOT_LIGHTS},
  {"KUKI_MAX_SPOT_SHADOW_LIGHTS", MAX_SPOT_SHADOW_LIGHTS},
};
} // namespace kuki
