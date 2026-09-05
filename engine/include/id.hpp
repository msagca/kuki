#pragma once
#include <functional>
#include <kuki_engine_export.h>
#include <string>
#include <string_view>
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
namespace detail {
  /// @brief FNV-1a over a name, seeded so one string can produce both halves of a UUID.
  inline constexpr auto Fnv1a64(const std::string_view text, uint64_t hash) -> uint64_t {
    constexpr uint64_t PRIME = 1099511628211ULL;
    for (const auto c : text) {
      hash ^= static_cast<uint8_t>(c);
      hash *= PRIME;
    }
    return hash;
  }
} // namespace detail
/// @brief The id a built-in asset always has, derived from its name.
///
/// A built-in asset has no file to be identified by, so a generated id would differ every run and
/// a scene saved yesterday would name ids that no longer exist. Deriving the id from the name makes
/// it a property of the asset rather than of the session, so a saved reference resolves on its own
/// and needs no search by name to rescue it.
///
/// Only assets a scene can reference need this. Shaders never reach a scene manifest, so they keep
/// generated ids, and the vertex stage of a shader pair has no name to derive one from anyway.
inline auto MakeBuiltInAssetID(const std::string_view name) -> AssetID {
  constexpr uint64_t HIGH_BASIS = 14695981039346656037ULL;
  constexpr uint64_t LOW_BASIS = 14695981039346656038ULL;
  return AssetID(UUID128{detail::Fnv1a64(name, HIGH_BASIS), detail::Fnv1a64(name, LOW_BASIS)});
}
/// @brief `EntityID::Invalid` as the picking targets store it, in the low bits of three channels.
///
/// The targets carry one byte per channel, so an id survives the round trip only in its low 24
/// bits. `EntityID::Invalid` is -1, which truncates to every one of those bits set, and the
/// renderers read that value back as the sentinel meaning nothing was drawn at a pixel. Zero
/// cannot serve the same purpose: it is `EntityID::First`, a real entity in any scene.
inline constexpr uint32_t ENTITY_ID_ENCODED_INVALID = 0xFFFFFFu;
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
