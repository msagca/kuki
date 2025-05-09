#define GLM_ENABLE_EXPERIMENTAL
#include <component/component.hpp>
#include <component/transform.hpp>
#include <glm/ext/quaternion_float.hpp>
#include <glm/ext/vector_float3.hpp>
#include <string>
#include <variant>
#include <vector>
namespace kuki {
const std::string Transform::GetName() const {
  return ComponentTraits<Transform>::GetName();
}
std::vector<Property> Transform::GetProperties() const {
  auto rotationEuler = glm::degrees(glm::eulerAngles(rotation));
  return {{"Position", position}, {"Rotation", rotationEuler}, {"Scale", scale, PropertyType::Scale}, {"Parent", parent}};
}
void Transform::SetProperty(Property property) {
  if (auto value = std::get_if<glm::vec3>(&property.value)) {
    if (property.name == "Position")
      position = *value;
    else if (property.name == "Rotation")
      rotation = glm::quat(glm::radians(*value));
    else if (property.name == "Scale")
      scale = *value;
    dirty = true; // TODO: propagate changes to children
  } else if (auto value = std::get_if<int>(&property.value)) {
    if (property.name == "Parent")
      parent = *value;
  }
}
} // namespace kuki
