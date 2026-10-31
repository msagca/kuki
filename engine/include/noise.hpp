#pragma once
#include <cstdint>
#include <glm/ext/vector_float2.hpp>
#include <kuki_engine_export.h>
namespace kuki {
/// @brief Gradient noise in two dimensions: a smooth hill-and-hollow field, about one feature a
/// unit, from about -1 to 1 and nought at every whole-numbered point.
///
/// Perlin's, with the gradients drawn from a hash of the lattice point and the seed rather than
/// from a table, so there is no table to shuffle and two seeds give unrelated fields. Deterministic:
/// the same point and seed give the same value on every machine.
KUKI_ENGINE_API auto GradientNoise(const glm::vec2 &, const uint32_t seed = 0u) -> float;
/// @brief Several octaves of gradient noise laid over one another, each `lacunarity` times finer and
/// `gain` times fainter than the last. Scaled back into about -1 to 1 whatever the octave count.
///
/// The usual way of turning a field with one size of feature into one with continents, bays and
/// coves at once.
KUKI_ENGINE_API auto FractalNoise(const glm::vec2 &, const int octaves = 4, const uint32_t seed = 0u, const float lacunarity = 2.f, const float gain = .5f) -> float;
/// @brief A ridged field: fractal noise folded at nought so its zeros become crests. Nought to one.
///
/// What a range of mountains looks like from above, where plain noise gives rounded hills.
KUKI_ENGINE_API auto RidgedNoise(const glm::vec2 &, const int octaves = 4, const uint32_t seed = 0u, const float lacunarity = 2.f, const float gain = .5f) -> float;
} // namespace kuki
