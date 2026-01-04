#include <hash_utils.hpp>
namespace kuki {
auto hash_combine(size_t &seed, size_t value) noexcept -> void {
  static constexpr size_t FNV_PRIME = 0x9e3779b97f4a7c15ULL;
  seed ^= value + FNV_PRIME + (seed << 6) + (seed >> 2);
}
} // namespace kuki
