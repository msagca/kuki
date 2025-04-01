#include "camera.hpp"
#include "light.hpp"
#include "texture.hpp"
#include "transform.hpp"
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
template <>
struct adl_serializer<Transform> {
  static void to_json(nlohmann::json& j, const Transform& obj) {
    j = nlohmann::json{{"Position", obj.position}, {"Rotation", obj.rotation}, {"Scale", obj.scale}, {"Parent", obj.parent}};
  }
  static void from_json(const nlohmann::json& j, Transform& obj) {
    j.at("Position").get_to(obj.position);
    j.at("Rotation").get_to(obj.rotation);
    j.at("Scale").get_to(obj.scale);
    j.at("Parent").get_to(obj.parent);
  }
};
template <>
struct adl_serializer<Camera> {
  static void to_json(nlohmann::json& j, const Camera& obj) {
    j = nlohmann::json{{"Type", obj.type}, {"Position", obj.position}, {"Front", obj.front}, {"Up", obj.up}, {"Right", obj.right}, {"Pitch", obj.pitch}, {"Yaw", obj.yaw}, {"FOV", obj.fov}, {"Aspect", obj.aspect}, {"Near", obj.near}, {"Far", obj.far}, {"Size", obj.size}};
  }
  static void from_json(const nlohmann::json& j, Camera& obj) {
    j.at("Type").get_to(obj.type);
    j.at("Position").get_to(obj.position);
    j.at("Front").get_to(obj.front);
    j.at("Up").get_to(obj.up);
    j.at("Right").get_to(obj.right);
    j.at("Pitch").get_to(obj.pitch);
    j.at("Yaw").get_to(obj.yaw);
    j.at("FOV").get_to(obj.fov);
    j.at("Aspect").get_to(obj.aspect);
    j.at("Near").get_to(obj.near);
    j.at("Far").get_to(obj.far);
    j.at("Type").get_to(obj.size);
  }
};
template <>
struct adl_serializer<Light> {
  static void to_json(nlohmann::json& j, const Light& obj) {
    j = nlohmann::json{{"Type", obj.type}, {"Vector", obj.vector}, {"Ambient", obj.ambient}, {"Diffuse", obj.diffuse}, {"Specular", obj.specular}, {"Constant", obj.constant}, {"Linear", obj.linear}, {"Quadratic", obj.quadratic}};
  }
  static void from_json(const nlohmann::json& j, Light& obj) {
    j.at("Type").get_to(obj.type);
    j.at("Vector").get_to(obj.vector);
    j.at("Ambient").get_to(obj.ambient);
    j.at("Diffuse").get_to(obj.diffuse);
    j.at("Specular").get_to(obj.specular);
    j.at("Constant").get_to(obj.constant);
    j.at("Linear").get_to(obj.linear);
    j.at("Quadratic").get_to(obj.quadratic);
  }
};
template <>
struct adl_serializer<Texture> {
  // FIXME: texture Id will change between runs, there is no point in serializing it; serialize the file path instead
  static void to_json(nlohmann::json& j, const Texture& obj) {
    j = nlohmann::json{{"Type", obj.type}, {"Id", obj.id}};
  }
  static void from_json(const nlohmann::json& j, Texture& obj) {
    j.at("Type").get_to(obj.type);
    j.at("Id").get_to(obj.id);
  }
};
} // namespace nlohmann
