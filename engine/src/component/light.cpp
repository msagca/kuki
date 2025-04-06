#include <component/component.hpp>
#include <component/light.hpp>
#include <component/transform.hpp>
#include <glm/ext/vector_float3.hpp>
#include <string>
#include <variant>
#include <vector>
#include <cmath>
#include <glm/ext/quaternion_float.hpp>
const std::string Light::GetName() const {
  return ComponentTraits<Light>::GetName();
}
std::vector<Property> Light::GetProperties() const {
  return {{"Type", type}, {"Vector", vector}, {"Ambient", ambient, PropertyType::Color}, {"Diffuse", diffuse, PropertyType::Color}, {"Specular", specular, PropertyType::Color}, {"Constant", constant}, {"Linear", linear}, {"Quadratic", quadratic}};
}
void Light::SetProperty(Property property) {
  if (std::holds_alternative<glm::vec3>(property.value)) {
    auto& value = std::get<glm::vec3>(property.value);
    if (property.name == "Vector")
      vector = value;
    else if (property.name == "Ambient")
      ambient = value;
    else if (property.name == "Diffuse")
      diffuse = value;
    else if (property.name == "Specular")
      specular = value;
  } else if (std::holds_alternative<float>(property.value)) {
    auto& value = std::get<float>(property.value);
    if (property.name == "Constant")
      constant = value;
    else if (property.name == "Linear")
      linear = value;
    else if (property.name == "Quadratic")
      quadratic = value;
  } else if (std::holds_alternative<LightType>(property.value))
    type = std::get<LightType>(property.value);
}
Transform Light::GetTransform() const {
  Transform transform;
  if (type == LightType::Directional) {
    transform.position = glm::vec3(.0f);
    if (glm::length(vector) > .0f) {
      auto normalizedDir = glm::normalize(vector);
      auto forward = glm::vec3(.0f, .0f, -1.0f);
      if (glm::abs(glm::dot(normalizedDir, forward)) > .9999f) {
        if (glm::dot(normalizedDir, forward) > 0)
          transform.rotation = glm::angleAxis(glm::pi<float>(), glm::vec3(.0f, 1.0f, .0f));
        else
          transform.rotation = glm::quat(1.0f, .0f, .0f, .0f);
      } else {
        auto rotationAxis = glm::cross(forward, normalizedDir);
        auto angle = std::acos(glm::dot(forward, normalizedDir));
        transform.rotation = glm::angleAxis(angle, glm::normalize(rotationAxis));
      }
    } else
      transform.rotation = glm::quat(1.0f, .0f, .0f, .0f);
    transform.scale = glm::vec3(1.0f);
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
