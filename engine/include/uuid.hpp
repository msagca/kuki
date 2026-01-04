#pragma once
#include <concepts.hpp>
#include <cstdint>
#include <functional>
#include <hash_utils.hpp>
#include <kuki_engine_export.h>
namespace kuki {
template <IsUUID T>
struct UUID {
  T value{T::Invalid};
  UUID() = default;
  explicit UUID(T);
  static const UUID Invalid;
  static UUID Generate();
  bool operator==(const UUID &) const noexcept = default;
  explicit operator bool() const;
  explicit operator int() const;
};
struct KUKI_ENGINE_API UUID128 {
  uint64_t high{0};
  uint64_t low{0};
  UUID128() = default;
  explicit UUID128(uint64_t, uint64_t);
  static const UUID128 Invalid;
  static UUID128 Generate();
  bool operator==(const UUID128 &) const = default;
  explicit operator bool() const;
  explicit operator int() const;
};
template <IsUUID T>
UUID<T>::UUID(T value)
  : value(value) {}
template <IsUUID T>
const UUID<T> UUID<T>::Invalid{T::Invalid};
template <IsUUID T>
UUID<T> UUID<T>::Generate() {
  UUID id;
  do
    id = UUID(T::Generate());
  while (!id);
  return id;
}
template <IsUUID T>
UUID<T>::operator bool() const {
  return *this != Invalid;
}
template <IsUUID T>
UUID<T>::operator int() const {
  return static_cast<int>(value);
}
} // namespace kuki
namespace std {
template <kuki::IsUUID T>
struct hash<kuki::UUID<T>> {
  size_t operator()(const kuki::UUID<T> &id) const noexcept {
    return hash<T>{}(id.value);
  }
};
template <>
struct hash<kuki::UUID128> {
  size_t operator()(const kuki::UUID128 &id) const noexcept {
    size_t h{};
    kuki::hash_combine(h, hash<uint64_t>{}(id.high));
    kuki::hash_combine(h, hash<uint64_t>{}(id.low));
    return h;
  }
};
} // namespace std
