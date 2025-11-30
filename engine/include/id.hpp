#pragma once
#include <kuki_engine_export.h>
#include <uuid.hpp>
namespace kuki {
template <typename T>
concept UniqueID = requires {
  { T::Generate() } -> std::same_as<T>;
  { T::Invalid() } -> std::same_as<T>;
};
template <UniqueID T>
struct KUKI_ENGINE_API UUID {
  T value{};
  UUID() = default;
  explicit UUID(T value);
  /// @brief Generate a non-zero ID
  static UUID Generate();
  static UUID Invalid();
  bool IsValid() const;
  bool operator==(const UUID&) const = default;
  explicit operator long() const;
  explicit operator long long() const;
};
template <UniqueID T>
UUID<T>::UUID(T value)
  : value(value) {}
template <UniqueID T>
UUID<T> UUID<T>::Generate() {
  UUID id;
  do
    id = UUID(T::Generate());
  while (!id.IsValid());
  return id;
}
template <UniqueID T>
UUID<T> UUID<T>::Invalid() {
  return UUID(T::Invalid());
}
template <UniqueID T>
bool UUID<T>::IsValid() const {
  return value.IsValid();
}
template <UniqueID T>
UUID<T>::operator long() const {
  return static_cast<long>(value);
}
template <UniqueID T>
UUID<T>::operator long long() const {
  return static_cast<long long>(value);
}
using ID = UUID<UUID64>;
} // namespace kuki
namespace std {
template <kuki::UniqueID T>
struct hash<kuki::UUID<T>> {
  size_t operator()(const kuki::UUID<T>& id) const noexcept {
    auto hasher = std::hash<T>{};
    return hasher(id.value);
  }
};
} // namespace std
