#pragma once
#include <cstdint>
namespace kuki {
/// @brief Threads in one group of the particle step, shared by the C++ that dispatches it and the
/// HLSL that runs it.
///
/// The C++ divides a pool by this to decide how many groups cover it, and the shader is compiled with
/// it as its group's width (`KUKI_PARTICLE_GROUP_SIZE`, through `SHADER_DEFINITIONS`). It was written
/// in both, each with a comment saying it mirrored the other; a pool sized for groups of one width
/// stepped by groups of another leaves its tail particles never updated, or steps past its end.
inline constexpr uint32_t PARTICLE_GROUP_SIZE = 64;
} // namespace kuki
