#pragma once
#include <glm/detail/type_vec3.hpp>
#include <glm/ext/quaternion_float.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <id.hpp>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
namespace nlohmann {
template <>
struct adl_serializer<kuki::UUID128> {
  static void to_json(nlohmann::json &j, const kuki::UUID128 &obj) {
    j = nlohmann::json{{"high", obj.high}, {"low", obj.low}};
  }
  static void from_json(const nlohmann::json &j, kuki::UUID128 &obj) {
    j.at("high").get_to(obj.high);
    j.at("low").get_to(obj.low);
  }
};
template <>
struct adl_serializer<kuki::AssetID> {
  static void to_json(nlohmann::json &j, const kuki::AssetID &obj) {
    j = obj.value;
  }
  static void from_json(const nlohmann::json &j, kuki::AssetID &obj) {
    obj = kuki::AssetID(j.get<kuki::UUID128>());
  }
};
template <>
struct adl_serializer<glm::vec2> {
  static void to_json(nlohmann::json &j, const glm::vec2 &obj) {
    j = nlohmann::json{{"x", obj.x}, {"y", obj.y}};
  }
  static void from_json(const nlohmann::json &j, glm::vec2 &obj) {
    j.at("x").get_to(obj.x);
    j.at("y").get_to(obj.y);
  }
};
template <>
struct adl_serializer<glm::vec3> {
  static void to_json(nlohmann::json &j, const glm::vec3 &obj) {
    j = nlohmann::json{{"x", obj.x}, {"y", obj.y}, {"z", obj.z}};
  }
  static void from_json(const nlohmann::json &j, glm::vec3 &obj) {
    j.at("x").get_to(obj.x);
    j.at("y").get_to(obj.y);
    j.at("z").get_to(obj.z);
  }
};
template <>
struct adl_serializer<glm::quat> {
  static void to_json(nlohmann::json &j, const glm::quat &obj) {
    j = nlohmann::json{{"w", obj.w}, {"x", obj.x}, {"y", obj.y}, {"z", obj.z}};
  }
  // A zero quaternion is read as the identity. `Transform::rotation` used to default to one, so
  // every file saved before that was fixed holds `{0, 0, 0, 0}` on an entity whose rotation was
  // never set, and meant no rotation by it: it is no rotation at all, so nothing else could be meant.
  static void from_json(const nlohmann::json &j, glm::quat &obj) {
    j.at("w").get_to(obj.w);
    j.at("x").get_to(obj.x);
    j.at("y").get_to(obj.y);
    j.at("z").get_to(obj.z);
    if (obj.w == .0f && obj.x == .0f && obj.y == .0f && obj.z == .0f)
      obj = glm::quat{1.f, .0f, .0f, .0f};
  }
};
} // namespace nlohmann
