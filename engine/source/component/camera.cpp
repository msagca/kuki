#include <component/camera.hpp>
#include <component/component.hpp>
#include <glm/ext/vector_float3.hpp>
#include <string>
#include <variant>
#include <vector>
const std::string Camera::GetName() const {
  return ComponentTraits<Camera>::GetName();
}
std::vector<Property> Camera::GetProperties() const {
  return {{"Type", type}, {"Position", position}, {"Pitch", pitch}, {"Yaw", yaw}, {"FOV", fov}, {"Aspect", aspect}, {"Near", near}, {"Far", far}, {"Size", size}};
}
void Camera::SetProperty(Property property) {
  if (std::holds_alternative<float>(property.value)) {
    auto& value = std::get<float>(property.value);
    if (property.name == "Pitch")
      pitch = value;
    else if (property.name == "Yaw")
      yaw = value;
    else if (property.name == "FOV")
      fov = value;
    else if (property.name == "Aspect")
      aspect = value;
    else if (property.name == "Near")
      near = value;
    else if (property.name == "Far")
      far = value;
    else if (property.name == "Size")
      size = value;
  } else if (std::holds_alternative<glm::vec3>(property.value)) {
    if (property.name == "Position")
      position = std::get<glm::vec3>(property.value);
  } else if (std::holds_alternative<CameraType>(property.value)) {
    type = std::get<CameraType>(property.value);
  }
}
