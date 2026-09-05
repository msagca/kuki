#pragma once
#include <cstdint>
namespace kuki {
/// @brief How many point lights one draw can be shaded by, shared by both backends and both
/// shading languages.
///
/// These sit here rather than in each backend for the reason the post-processing constants do, and
/// with more at stake. A light limit is written down in four places at once: the C++ that gathers
/// the lights and sizes the constant buffer, the GLSL that loops over them, the HLSL that does the
/// same, and the HLSL the probe trace shades its rays with. It used to be written down in all four
/// independently -- `dx_pipeline.hpp` held the two counts, `gl_renderer.hpp` held the shadow count,
/// `lit.frag` and `scene.hlsl` and `probe_trace.hlsl` each declared their own -- held in agreement
/// by a comment asking the next person to keep them in step.
///
/// What made that worse than the usual duplication is how it fails. Raising the count in the C++
/// alone does not overflow anything: the gather clamps to the array it is filling, so the extra
/// lights are collected, counted, and then quietly dropped. Nothing asserts, nothing logs, and the
/// scene simply stops lighting from a light that is plainly there in the hierarchy. The two shader
/// copies fail the same way in reverse.
///
/// So the shaders no longer declare these at all. Both compilers take their source as a string at
/// runtime, which means both can be handed the values as preprocessor definitions -- see
/// `SHADER_DEFINITIONS` in `shader_definitions.hpp`, with `DXShaderCompiler::Compile` carrying them
/// to the HLSL side and `GLShaderBase::Compile` to the GLSL one. A shader that reads one of these
/// without being given it fails to compile with a message naming the macro, which is the loud
/// failure the comment was standing in for.
inline constexpr uint32_t MAX_POINT_LIGHTS = 8;
/// @brief How many spot lights one draw can be shaded by, on the same footing.
inline constexpr uint32_t MAX_SPOT_LIGHTS = 8;
/// @brief How many of those spot lights get a shadow map layer of their own.
///
/// Separate from the count above because a shadow costs a depth pass and a rendered layer where
/// being shaded costs a few instructions, so there is a real reason to want fewer of these than of
/// those. It cannot be the larger of the two: a spot light with a layer that nothing shades is a
/// map drawn and never read.
///
/// This one has a second job. The render graph sizes the `SpotShadowMap` texture array from it, and
/// the graph is shared, so every backend's layer count comes from here whether it wanted to say so
/// or not. That is precisely why it cannot live in `gl_renderer.hpp`, which is where it used to.
inline constexpr uint32_t MAX_SPOT_SHADOW_LIGHTS = 8;
static_assert(MAX_SPOT_SHADOW_LIGHTS <= MAX_SPOT_LIGHTS, "a spot light with a shadow map layer nobody shades is a map drawn and never read");
} // namespace kuki
