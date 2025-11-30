#pragma once
#include <cstdint>
#include <functional>
#include <kuki_engine_export.h>
#include <string>
#include <string_view>
namespace kuki {
struct KUKI_ENGINE_API UUID64 {
  std::uint64_t value{0};
  UUID64() = default;
  UUID64(std::uint64_t);
  /// @brief Generate a random v4 UUID
  static UUID64 Generate();
  /// @brief A value to indicate the ID is invalid
  static UUID64 Invalid();
  bool IsValid() const;
  bool operator==(const UUID64&) const = default;
  explicit operator long() const;
  explicit operator long long() const;
};
} // namespace kuki
namespace std {
template <>
struct hash<kuki::UUID64> {
  size_t operator()(kuki::UUID64 const& id) const noexcept {
    return std::hash<std::uint64_t>{}(id.value);
  }
};
} // namespace std
