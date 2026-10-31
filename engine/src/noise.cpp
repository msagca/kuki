#include <cmath>
#include <noise.hpp>
#include <random.hpp>
namespace kuki {
namespace {
  /// @brief One of eight unit gradients, picked by a lattice point's hash. Eight directions are
  /// enough that no axis shows; the diagonals are scaled to unit length like the rest.
  auto Gradient(const int x, const int y, const uint32_t seed, const float dx, const float dy) -> float {
    constexpr float D = .70710678f;
    switch (HashPoint(x, y, seed) & 7u) {
    case 0:
      return dx;
    case 1:
      return -dx;
    case 2:
      return dy;
    case 3:
      return -dy;
    case 4:
      return D * (dx + dy);
    case 5:
      return D * (dx - dy);
    case 6:
      return D * (-dx + dy);
    default:
      return D * (-dx - dy);
    }
  }
  /// @brief The quintic fade, whose first and second derivatives are nought at both ends, so the
  /// field has no crease along the lattice lines.
  auto Fade(const float t) -> float {
    return t * t * t * (t * (t * 6.f - 15.f) + 10.f);
  }
  auto Lerp(const float a, const float b, const float t) -> float {
    return a + (b - a) * t;
  }
} // namespace
auto GradientNoise(const glm::vec2 &point, const uint32_t seed) -> float {
  const auto fx = std::floor(point.x);
  const auto fy = std::floor(point.y);
  const auto x = static_cast<int>(fx);
  const auto y = static_cast<int>(fy);
  const auto dx = point.x - fx;
  const auto dy = point.y - fy;
  const auto n00 = Gradient(x, y, seed, dx, dy);
  const auto n10 = Gradient(x + 1, y, seed, dx - 1.f, dy);
  const auto n01 = Gradient(x, y + 1, seed, dx, dy - 1.f);
  const auto n11 = Gradient(x + 1, y + 1, seed, dx - 1.f, dy - 1.f);
  const auto u = Fade(dx);
  const auto v = Fade(dy);
  // The largest a two-dimensional Perlin field reaches is the square root of a half, so this puts
  // the extremes at about one.
  return Lerp(Lerp(n00, n10, u), Lerp(n01, n11, u), v) * 1.41421356f;
}
auto FractalNoise(const glm::vec2 &point, const int octaves, const uint32_t seed, const float lacunarity, const float gain) -> float {
  auto sum = .0f;
  auto amplitude = 1.f;
  auto total = .0f;
  auto frequency = 1.f;
  for (auto octave = 0; octave < octaves; ++octave) {
    // Each octave from a seed of its own, so the octaves' zeros do not line up at the origin.
    sum += amplitude * GradientNoise(point * frequency, seed + static_cast<uint32_t>(octave) * 0x9e3779b9u);
    total += amplitude;
    amplitude *= gain;
    frequency *= lacunarity;
  }
  return total > .0f ? sum / total : .0f;
}
auto RidgedNoise(const glm::vec2 &point, const int octaves, const uint32_t seed, const float lacunarity, const float gain) -> float {
  auto sum = .0f;
  auto amplitude = 1.f;
  auto total = .0f;
  auto frequency = 1.f;
  for (auto octave = 0; octave < octaves; ++octave) {
    const auto ridge = 1.f - std::abs(GradientNoise(point * frequency, seed + static_cast<uint32_t>(octave) * 0x9e3779b9u));
    sum += amplitude * ridge * ridge;
    total += amplitude;
    amplitude *= gain;
    frequency *= lacunarity;
  }
  return total > .0f ? sum / total : .0f;
}
} // namespace kuki
