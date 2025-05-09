#include <component/component.hpp>
#include <component/light.hpp>
#include <component/transform.hpp>
#include <glm/ext/quaternion_float.hpp>
#include <glm/ext/vector_float3.hpp>
#include <string>
#include <variant>
#include <vector>
namespace kuki {
const std::string Light::GetName() const {
  return ComponentTraits<Light>::GetName();
}
std::vector<Property> Light::GetProperties() const {
  return {{"Type", type}, {"Vector", vector}, {"Ambient", ambient, PropertyType::Color}, {"Diffuse", diffuse, PropertyType::Color}, {"Specular", specular, PropertyType::Color}, {"Constant", constant}, {"Linear", linear}, {"Quadratic", quadratic}};
}
void Light::SetProperty(Property property) {
  if (auto value = std::get_if<glm::vec3>(&property.value)) {
    if (property.name == "Vector")
      vector = *value;
    else if (property.name == "Ambient")
      ambient = *value;
    else if (property.name == "Diffuse")
      diffuse = *value;
    else if (property.name == "Specular")
      specular = *value;
  } else if (auto value = std::get_if<float>(&property.value)) {
    if (property.name == "Constant")
      constant = *value;
    else if (property.name == "Linear")
      linear = *value;
    else if (property.name == "Quadratic")
      quadratic = *value;
  } else if (auto value = std::get_if<LightType>(&property.value)) {
    if (property.name == "Type")
      type = *value;
  }
}
Transform Light::GetTransform() const {
  static const auto WORLD_UP = glm::vec3(.0f, 1.0f, .0f);
  static const auto WORLD_BACK = glm::vec3(.0f, .0f, 1.0f);
  static const auto PARALLEL_THRESHOLD = .9999f;
  Transform transform;
  if (type == LightType::Directional) {
    transform.position = glm::vec3(.0f);
    auto direction = glm::normalize(vector);
    auto up = WORLD_UP;
    if (glm::abs(glm::dot(direction, up)) > PARALLEL_THRESHOLD)
      up = WORLD_BACK;
    transform.rotation = glm::quatLookAt(direction, up);
  } else if (type == LightType::Point) {
    transform.position = vector;
    transform.rotation = glm::quat(1.0f, .0f, .0f, .0f);
  }
  return transform;
}
void Light::SetTransform(const Transform& transform) {
  if (type == LightType::Directional) {
    auto forward = glm::vec3(.0f, .0f, -1.0f);
    vector = glm::normalize(transform.rotation * forward);
  } else if (type == LightType::Point)
    vector = transform.position;
}
} // namespace kuki
