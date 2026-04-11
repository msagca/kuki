#pragma once
#include <functional>
#include <kuki_engine_export.h>
#include <string>
#include <uuid.hpp>
namespace kuki {
template <typename T>
struct ID {
public:
  ID() = default;
  explicit ID(long long);
  static const ID First;
  static const ID Invalid;
  auto ToString() const -> std::string;
  auto operator++() -> ID &;
  auto operator++(int) -> ID;
  auto operator+=(long long) -> ID &;
  auto operator<(const ID &) const noexcept -> bool = default;
  auto operator==(const ID &) const noexcept -> bool = default;
  explicit operator bool() const;
  explicit operator int() const;
  operator long long() const;
  // NOTE: the following is needed for ImGui
  explicit operator const void *() const;
  explicit operator void *();
private:
  long long value{Invalid};
};
template <typename T>
ID<T>::ID(long long value)
  : value(value) {}
template <typename T>
const ID<T> ID<T>::First{0};
template <typename T>
const ID<T> ID<T>::Invalid{-1};
template <typename T>
std::string ID<T>::ToString() const {
  return std::to_string(value);
}
template <typename T>
auto ID<T>::operator++() -> ID<T> & {
  ++value;
  return *this;
}
template <typename T>
auto ID<T>::operator++(int) -> ID<T> {
  ID<T> temp = *this;
  ++*this;
  return temp;
}
template <typename T>
auto ID<T>::operator+=(long long increment) -> ID<T> & {
  value += increment;
  return *this;
}
template <typename T>
ID<T>::operator bool() const {
  return *this != Invalid;
}
template <typename T>
ID<T>::operator const void *() const {
  return static_cast<const void *>(&value);
}
template <typename T>
ID<T>::operator int() const {
  return static_cast<int>(value);
}
template <typename T>
ID<T>::operator void *() {
  return static_cast<void *>(&value);
}
template <typename T>
ID<T>::operator long long() const {
  return value;
}
using AssetID = UUID<UUID128>;
using EntityID = ID<class Entity>;
using GenCount = ID<class Generation>;
using PassID = ID<class Pass>;
using SceneID = ID<class Scene>;
} // namespace kuki
namespace std {
template <>
struct hash<kuki::EntityID> {
  auto operator()(const kuki::EntityID &id) const noexcept -> size_t {
    return hash<long long>{}(id);
  }
};
template <>
struct hash<kuki::SceneID> {
  auto operator()(const kuki::SceneID &id) const noexcept -> size_t {
    return hash<long long>{}(id);
  }
};
template <>
struct hash<kuki::PassID> {
  auto operator()(const kuki::PassID &id) const noexcept -> size_t {
    return hash<long long>{}(id);
  }
};
} // namespace std
