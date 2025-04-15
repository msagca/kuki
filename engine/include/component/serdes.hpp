#include <glm/detail/type_vec3.hpp>
#include <glm/ext/vector_float3.hpp>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
namespace nlohmann {
template <>
struct adl_serializer<glm::vec3> {
  static void to_json(nlohmann::json& j, const glm::vec3& obj) {
    j = nlohmann::json{{"x", obj.x}, {"y", obj.y}, {"z", obj.z}};
  }
  static void from_json(const nlohmann::json& j, glm::vec3& obj) {
    j.at("x").get_to(obj.x);
    j.at("y").get_to(obj.y);
    j.at("z").get_to(obj.z);
  }
};
template <>
struct adl_serializer<glm::quat> {
  static void to_json(nlohmann::json& j, const glm::quat& obj) {
    j = nlohmann::json{{"w", obj.w}, {"x", obj.x}, {"y", obj.y}, {"z", obj.z}};
  }
  static void from_json(const nlohmann::json& j, glm::quat& obj) {
    j.at("w").get_to(obj.w);
    j.at("x").get_to(obj.x);
    j.at("y").get_to(obj.y);
    j.at("z").get_to(obj.z);
  }
};
