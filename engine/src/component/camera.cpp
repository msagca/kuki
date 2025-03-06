#include <component/camera.hpp>
#include <component/component.hpp>
#include <glm/ext/vector_float3.hpp>
#include <string>
#include <variant>
#include <vector>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <cmath>
const std::string Camera::GetName() const {
  return ComponentTraits<Camera>::GetName();
}
std::vector<Property> Camera::GetProperties() const {
  return {{"Type", type}, {"Position", position}, {"Pitch", pitch}, {"Yaw", yaw}, {"FOV", fov}, {"AspectRatio", aspectRatio}, {"NearPlane", nearPlane}, {"FarPlane", farPlane}, {"OrthoSize", orthoSize}};
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
    else if (property.name == "AspectRatio")
      aspectRatio = value;
    else if (property.name == "NearPlane")
      nearPlane = value;
    else if (property.name == "FarPlane")
      farPlane = value;
    else if (property.name == "OrthoSize")
      orthoSize = value;
  } else if (std::holds_alternative<glm::vec3>(property.value)) {
    position = std::get<glm::vec3>(property.value);
  } else if (std::holds_alternative<CameraType>(property.value)) {
    type = std::get<CameraType>(property.value);
  }
}
void Camera::SetAspectRatio(float value) {
  aspectRatio = value;
  UpdateProjection();
}
void Camera::UpdateDirection() {
  static const auto WORLD_UP = glm::vec3(.0f, 1.0f, .0f);
  front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
  front.y = sin(glm::radians(pitch));
  front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
  front = glm::normalize(front);
  right = glm::normalize(glm::cross(front, WORLD_UP));
  up = glm::normalize(glm::cross(right, front));
}
void Camera::UpdateView() {
  view = glm::lookAt(position, position + front, up);
}
void Camera::UpdateProjection() {
  if (type == CameraType::Perspective)
    projection = glm::perspective(fov, aspectRatio, nearPlane, farPlane);
  else {
    auto left = orthoSize * aspectRatio;
    auto bottom = orthoSize;
    projection = glm::ortho(left, -left, bottom, -bottom, nearPlane, farPlane);
  }
}
