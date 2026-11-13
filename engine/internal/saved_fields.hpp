#pragma once
#include <component_fields.hpp>
#include <cstdint>
#include <cstring>
#include <glm/ext/vector_float4.hpp>
#include <id.hpp>
#include <nlohmann/json.hpp>
#include <serdes.hpp>
#include <span>
#include <spdlog/spdlog.h>
#include <string>
#include <unordered_map>
#include <unordered_set>
/// A game's component in a scene file: the members it declared (`ComponentFields`) written by key,
/// as the serializer writes an engine component's, and read back the same way.
///
/// Apart from the serializer so the round trip can be tested without a window: these need nothing
/// but a component's bytes and its fields. Inline rather than exported, since only the serializer
/// and the tests use them, and both are built against the same nlohmann.
namespace kuki {
namespace detail {
  template <typename T>
  auto LoadBytes(const void *at) -> T {
    T value;
    std::memcpy(&value, at, sizeof(T));
    return value;
  }
  template <typename T>
  auto StoreBytes(void *at, const T value) -> void {
    std::memcpy(at, &value, sizeof(T));
  }
  // Integers and enums through their bytes rather than a cast pointer: an enum's member read as its
  // underlying integer type is not the same object type, and a copy is what every compiler agrees on.
  inline auto LoadSigned(const void *at, const uint8_t size) -> int64_t {
    switch (size) {
    case 1:
      return LoadBytes<int8_t>(at);
    case 2:
      return LoadBytes<int16_t>(at);
    case 4:
      return LoadBytes<int32_t>(at);
    default:
      return LoadBytes<int64_t>(at);
    }
  }
  inline auto StoreSigned(void *at, const uint8_t size, const int64_t value) -> void {
    switch (size) {
    case 1:
      return StoreBytes(at, static_cast<int8_t>(value));
    case 2:
      return StoreBytes(at, static_cast<int16_t>(value));
    case 4:
      return StoreBytes(at, static_cast<int32_t>(value));
    default:
      return StoreBytes(at, value);
    }
  }
  inline auto LoadUnsigned(const void *at, const uint8_t size) -> uint64_t {
    switch (size) {
    case 1:
      return LoadBytes<uint8_t>(at);
    case 2:
      return LoadBytes<uint16_t>(at);
    case 4:
      return LoadBytes<uint32_t>(at);
    default:
      return LoadBytes<uint64_t>(at);
    }
  }
  inline auto StoreUnsigned(void *at, const uint8_t size, const uint64_t value) -> void {
    switch (size) {
    case 1:
      return StoreBytes(at, static_cast<uint8_t>(value));
    case 2:
      return StoreBytes(at, static_cast<uint16_t>(value));
    case 4:
      return StoreBytes(at, static_cast<uint32_t>(value));
    default:
      return StoreBytes(at, value);
    }
  }
} // namespace detail
/// @brief A component's saved fields as a JSON object, one key each.
///
/// An entity field is written as the place in the file of the entity it names (`order`), -1 for
/// none or for one not in the file; an asset field as its id, which is added to `referenced` so the
/// scene's manifest lists it.
inline auto SavedFieldsToJson(const std::span<const ComponentField> fields, void *component, const std::unordered_map<EntityID, int> &order, std::unordered_set<AssetID> &referenced) -> nlohmann::json {
  auto out = nlohmann::json::object();
  for (const auto &field : fields) {
    auto *at = field.address(component);
    const std::string key(field.key);
    switch (field.kind) {
    case FieldKind::Bool:
      out[key] = detail::LoadBytes<bool>(at);
      break;
    case FieldKind::Signed:
      out[key] = detail::LoadSigned(at, field.size);
      break;
    case FieldKind::Unsigned:
      out[key] = detail::LoadUnsigned(at, field.size);
      break;
    case FieldKind::Float:
      out[key] = field.size == sizeof(double) ? detail::LoadBytes<double>(at) : detail::LoadBytes<float>(at);
      break;
    case FieldKind::Vec2:
      out[key] = *static_cast<const glm::vec2 *>(at);
      break;
    case FieldKind::Vec3:
      out[key] = *static_cast<const glm::vec3 *>(at);
      break;
    case FieldKind::Vec4: {
      const auto &v = *static_cast<const glm::vec4 *>(at);
      out[key] = {{"x", v.x}, {"y", v.y}, {"z", v.z}, {"w", v.w}};
      break;
    }
    case FieldKind::Quat:
      out[key] = *static_cast<const glm::quat *>(at);
      break;
    case FieldKind::Entity: {
      const auto it = order.find(*static_cast<const EntityID *>(at));
      out[key] = it != order.end() ? it->second : -1;
      break;
    }
    case FieldKind::Asset: {
      const auto &id = *static_cast<const AssetID *>(at);
      if (id)
        referenced.insert(id);
      out[key] = id;
      break;
    }
    }
  }
  return out;
}
/// @brief Reads what a file has of a component's saved fields into it.
///
/// A key the file lacks leaves its field at the value it has, which on a new entity is its default:
/// that is how a field added after the scene was saved loads. A key the component no longer has is
/// ignored, and one of the wrong shape is reported and skipped. An entity field is left holding the
/// place in the file it was saved as, which `ResolveSavedEntities` turns into an id once every
/// entity of the file has been made; an asset field is moved to the id the manifest remapped it to.
inline auto SavedFieldsFromJson(const nlohmann::json &in, const std::span<const ComponentField> fields, void *component, const std::unordered_map<AssetID, AssetID> &remap) -> void {
  if (!in.is_object())
    return;
  for (const auto &field : fields) {
    const std::string key(field.key);
    const auto found = in.find(key);
    if (found == in.end())
      continue;
    auto *at = field.address(component);
    try {
      switch (field.kind) {
      case FieldKind::Bool:
        detail::StoreBytes(at, found->get<bool>());
        break;
      case FieldKind::Signed:
        detail::StoreSigned(at, field.size, found->get<int64_t>());
        break;
      case FieldKind::Unsigned:
        detail::StoreUnsigned(at, field.size, found->get<uint64_t>());
        break;
      case FieldKind::Float:
        if (field.size == sizeof(double))
          detail::StoreBytes(at, found->get<double>());
        else
          detail::StoreBytes(at, found->get<float>());
        break;
      case FieldKind::Vec2:
        found->get_to(*static_cast<glm::vec2 *>(at));
        break;
      case FieldKind::Vec3:
        found->get_to(*static_cast<glm::vec3 *>(at));
        break;
      case FieldKind::Vec4: {
        auto &v = *static_cast<glm::vec4 *>(at);
        v = {found->at("x").get<float>(), found->at("y").get<float>(), found->at("z").get<float>(), found->at("w").get<float>()};
        break;
      }
      case FieldKind::Quat:
        found->get_to(*static_cast<glm::quat *>(at));
        break;
      case FieldKind::Entity:
        *static_cast<EntityID *>(at) = EntityID{found->get<long long>()};
        break;
      case FieldKind::Asset: {
        auto id = found->get<AssetID>();
        if (const auto it = remap.find(id); it != remap.end())
          id = it->second;
        *static_cast<AssetID *>(at) = id;
        break;
      }
      }
    } catch (const nlohmann::json::exception &error) {
      spdlog::warn("[SceneSerializer] Saved field '{}' is not of the shape its component reads, and keeps its default: {}", key, error.what());
    }
  }
}
/// @brief Whether any of a component's saved fields names an entity, and so has to wait for
/// `ResolveSavedEntities`.
inline auto NamesEntities(const std::span<const ComponentField> fields) -> bool {
  for (const auto &field : fields)
    if (field.kind == FieldKind::Entity)
      return true;
  return false;
}
/// @brief Swaps the places in the file the entity fields were read as for the entities made from
/// them: `created` is every entity of the file, in the order the file has them.
inline auto ResolveSavedEntities(const std::span<const ComponentField> fields, void *component, const std::span<const EntityID> created) -> void {
  for (const auto &field : fields) {
    if (field.kind != FieldKind::Entity)
      continue;
    auto &id = *static_cast<EntityID *>(field.address(component));
    const auto place = static_cast<long long>(id);
    id = place >= 0 && place < static_cast<long long>(created.size()) ? created[static_cast<size_t>(place)] : EntityID::Invalid;
  }
}
} // namespace kuki
