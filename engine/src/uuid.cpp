#include <cstdint>
#include <random>
#include <uuid.hpp>
namespace kuki {
UUID64::UUID64(std::uint64_t v)
  : value(v) {}
UUID64 UUID64::Generate() {
  static thread_local std::mt19937_64 rng{std::random_device{}()};
  std::uniform_int_distribution<std::uint64_t> dist;
  return UUID64{dist(rng)};
}
UUID64 UUID64::Invalid() {
  return {0};
}
bool UUID64::IsValid() const {
  return value != 0;
}
UUID64::operator long() const {
  return static_cast<long>(value);
}
UUID64::operator long long() const {
  return static_cast<long long>(value);
}
} // namespace kuki
