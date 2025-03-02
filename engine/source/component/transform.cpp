#include <component/component.hpp>
#include <component/transform.hpp>
#include <glm/ext/vector_float3.hpp>
#include <string>
#include <variant>
#include <vector>
const std::string Transform::GetName() const {
  return ComponentTraits<Transform>::GetName();
}
std::vector<Property> Transform::GetProperties() const {
  return {{"Position", position}, {"Rotation", rotation}, {"Scale", scale}, {"Parent", parent}};
}
void Transform::SetProperty(Property property) {
  if (std::holds_alternative<glm::vec3>(property.value)) {
    auto& value = std::get<glm::vec3>(property.value);
    if (property.name == "Position")
      position = value;
    else if (property.name == "Rotation")
      rotation = value;
    else if (property.name == "Scale")
      scale = value;
  } else if (std::holds_alternative<int>(property.value)) {
    auto& value = std::get<int>(property.value);
    if (property.name == "Parent")
      parent = value;
  }
}
