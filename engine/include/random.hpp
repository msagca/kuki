#pragma once
#include <cstdint>
namespace kuki {
/// @brief A small generator of random numbers that gives the same sequence on every machine.
///
/// PCG32: sixty-four bits of state and a stream, thirty-two bits out. Written out here rather than
/// taken from `<random>`, because the standard fixes what its engines produce but not what its
/// distributions make of it -- `std::uniform_real_distribution` is free to draw differently under
/// another standard library -- and anything procedural is only worth a seed if the seed makes the
/// same thing everywhere. A map two players generate from one seed has to be one map.
///
/// Floats are made from the top twenty-four bits alone, so they are exact and the same on any
/// hardware that has a float at all.
class Random {
public:
  constexpr explicit Random(const uint64_t seed = 0x853c49e6748fea9bull, const uint64_t stream = 0xda3e39cb94b95bdbull)
    : increment((stream << 1u) | 1u) {
    Next();
    state += seed;
    Next();
  }
  /// @brief Thirty-two random bits.
  constexpr auto Next() -> uint32_t {
    const auto old = state;
    state = old * 6364136223846793005ull + increment;
    const auto shifted = static_cast<uint32_t>(((old >> 18u) ^ old) >> 27u);
    const auto rotation = static_cast<uint32_t>(old >> 59u);
    return (shifted >> rotation) | (shifted << ((~rotation + 1u) & 31u));
  }
  /// @brief A number in [0, 1).
  constexpr auto Float() -> float {
    return static_cast<float>(Next() >> 8u) * (1.f / 16777216.f);
  }
  /// @brief A number in [low, high).
  constexpr auto Range(const float low, const float high) -> float {
    return low + (high - low) * Float();
  }
  /// @brief A whole number in [0, bound), without the bias a plain remainder has.
  constexpr auto Below(const uint32_t bound) -> uint32_t {
    if (bound <= 1u)
      return 0u;
    const auto threshold = (~bound + 1u) % bound;
    for (;;)
      if (const auto value = Next(); value >= threshold)
        return value % bound;
  }
  /// @brief A whole number in [low, high].
  constexpr auto Between(const int low, const int high) -> int {
    return high <= low ? low : low + static_cast<int>(Below(static_cast<uint32_t>(high - low + 1)));
  }
  /// @brief Whether a thing that happens `chance` of the time happened.
  constexpr auto Chance(const float chance) -> bool {
    return Float() < chance;
  }
private:
  uint64_t state{};
  uint64_t increment{};
};
/// @brief Thirty-two well-mixed bits from a number, for hashing a position or an index into a seed.
///
/// The finaliser of MurmurHash3. Every bit of the input reaches every bit of the output, so two
/// neighbouring cells or two consecutive indices give numbers with nothing in common.
constexpr auto MixBits(uint32_t value) -> uint32_t {
  value ^= value >> 16u;
  value *= 0x85ebca6bu;
  value ^= value >> 13u;
  value *= 0xc2b2ae35u;
  value ^= value >> 16u;
  return value;
}
/// @brief Thirty-two bits from a seed and two whole numbers -- a lattice point, or a cell and a
/// layer -- that differ for every one of the three.
constexpr auto HashPoint(const int x, const int y, const uint32_t seed = 0u) -> uint32_t {
  return MixBits(static_cast<uint32_t>(x) * 0x8da6b343u ^ MixBits(static_cast<uint32_t>(y) * 0xd8163841u ^ seed * 0xcb1ab31fu));
}
} // namespace kuki
