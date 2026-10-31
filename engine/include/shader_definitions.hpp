#pragma once
#include <cstdint>
#include <light_limits.hpp>
#include <particle_emitter.hpp>
#include <particle_limits.hpp>
#include <probe_limits.hpp>
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
/// No shader is compiled ahead of time, and that is what makes this fixable. The HLSL goes through
/// `dxcompiler.dll` at runtime, so it is taken as a string and can be handed definitions alongside
/// it -- as a second backend's shaders must be, whether HLSL through dxc or another language. The shader declares
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
/// number. HLSL is happy either way; the suffix was for the GLSL the OpenGL backend had, and stays
/// so that any language a later backend brings is given a form it accepts.
inline constexpr ShaderDefinition SHADER_DEFINITIONS[]{
  {"KUKI_MAX_POINT_LIGHTS", MAX_POINT_LIGHTS},
  {"KUKI_MAX_SPOT_LIGHTS", MAX_SPOT_LIGHTS},
  {"KUKI_MAX_SPOT_SHADOW_LIGHTS", MAX_SPOT_SHADOW_LIGHTS},
  {"KUKI_PROBE_OCTREE_MAX_DEPTH", PROBE_OCTREE_MAX_DEPTH},
  {"KUKI_PROBE_MEASURE_LINES_PER_THREAD", PROBE_MEASURE_LINES_PER_THREAD},
  {"KUKI_PARTICLE_GROUP_SIZE", PARTICLE_GROUP_SIZE},
  // An enumerator and not a count, but the shader compares a `uint` against it all the same.
  {"KUKI_PARTICLE_SHAPE_MESH", static_cast<uint32_t>(ParticleShape::Mesh)},
  {"KUKI_PARTICLE_SHAPE_BOX", static_cast<uint32_t>(ParticleShape::Box)},
  {"KUKI_PARTICLE_FACING_VELOCITY", static_cast<uint32_t>(ParticleFacing::Velocity)},
  {"KUKI_PARTICLE_FACING_FLAT", static_cast<uint32_t>(ParticleFacing::Flat)},
  {"KUKI_PARTICLE_SPRITE_RING", static_cast<uint32_t>(ParticleSprite::Ring)},
  {"KUKI_PARTICLE_SPRITE_PETAL", static_cast<uint32_t>(ParticleSprite::Petal)},
};
} // namespace kuki
