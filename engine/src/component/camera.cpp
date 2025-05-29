#include <cmath>
#include <component/camera.hpp>
#include <component/component.hpp>
#include <component/mesh.hpp>
#include <component/transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_float3x3.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/vector_float3.hpp>
#include <string>
#include <variant>
#include <vector>
namespace kuki {
bool Plane::OnPlane(const glm::vec3& center, const glm::vec3& extents) const {
  const auto r = glm::dot(glm::abs(normal), extents);
  return SignedDistance(center) + r >= 0;
}
float Plane::SignedDistance(const glm::vec3& point) const {
  return glm::dot(normal, point - this->point);
}
bool Frustum::OverlapsFrustum(const BoundingBox& bounds) const {
  auto center = (bounds.min + bounds.max) * .5f;
  glm::vec3 extents{bounds.max.x - center.x, bounds.max.y - center.y, bounds.max.z - center.z};
  return near.OnPlane(center, extents) && far.OnPlane(center, extents) && right.OnPlane(center, extents) && left.OnPlane(center, extents) && top.OnPlane(center, extents) && bottom.OnPlane(center, extents);
}
const std::string Camera::GetName() const {
  return ComponentTraits<Camera>::GetName();
}
std::vector<Property> Camera::GetProperties() const {
  return {{"Type", type}, {"Position", position}, {"Pitch", glm::degrees(pitch)}, {"Yaw", glm::degrees(yaw)}, {"FOV", fov}, {"AspectRatio", aspectRatio}, {"NearPlane", nearPlane}, {"FarPlane", farPlane}, {"OrthoSize", orthoSize}};
}
void Camera::SetProperty(Property property) {
  if (auto value = std::get_if<float>(&property.value)) {
    if (property.name == "Pitch")
      pitch = glm::radians(*value);
    else if (property.name == "Yaw")
      yaw = glm::radians(*value);
    else if (property.name == "FOV")
      fov = *value;
    else if (property.name == "AspectRatio")
      aspectRatio = *value;
    else if (property.name == "NearPlane")
      nearPlane = *value;
    else if (property.name == "FarPlane")
      farPlane = *value;
    else if (property.name == "OrthoSize")
      orthoSize = *value;
    UpdateDirection();
    UpdateView();
    UpdateProjection();
  } else if (auto value = std::get_if<glm::vec3>(&property.value)) {
    position = *value;
    UpdateView();
  } else if (auto value = std::get_if<CameraType>(&property.value)) {
    type = *value;
    UpdateProjection();
  }
  UpdateFrustum();
}
Transform Camera::GetTransform() const {
  Transform transform;
  transform.position = position;
  glm::mat3 rotation(right, up, front);
  transform.rotation = glm::quat_cast(rotation);
  return transform;
}
void Camera::SetTransform(const Transform& transform) {
  position = transform.position;
  auto rotation = glm::mat3_cast(transform.rotation);
  right = rotation[0];
  up = rotation[1];
  front = rotation[2];
  auto euler = glm::eulerAngles(transform.rotation);
  pitch = euler.x;
  yaw = euler.y;
  UpdateView();
  UpdateFrustum();
}
void Camera::Update() {
  UpdateDirection();
  UpdateView();
  UpdateProjection();
  UpdateFrustum();
}
void Camera::UpdateDirection() {
  static const auto WORLD_UP = glm::vec3(.0f, 1.0f, .0f);
  front.x = cos(yaw) * cos(pitch);
  front.y = sin(pitch);
  front.z = sin(yaw) * cos(pitch);
  front = glm::normalize(front);
  right = glm::normalize(glm::cross(front, WORLD_UP));
  up = glm::normalize(glm::cross(right, front));
}
void Camera::UpdateView() {
  view = glm::lookAt(position, position + front, up);
}
void Camera::UpdateProjection() {
  if (type == CameraType::Perspective)
    projection = glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane);
  else {
    auto left = -orthoSize * aspectRatio;
    auto bottom = -orthoSize;
    projection = glm::ortho(left, -left, bottom, -bottom, nearPlane, farPlane);
  }
}
void Camera::UpdateFrustum() {
  const auto vHalf = farPlane * tanf(glm::radians(fov) * .5f);
  const auto hHalf = vHalf * aspectRatio;
  const auto farPos = farPlane * front;
  frustum.near = {position + nearPlane * front, front};
  frustum.far = {position + farPos, -front};
  frustum.right = {position, glm::normalize(glm::cross(farPos - right * hHalf, up))};
  frustum.left = {position, glm::normalize(glm::cross(up, farPos + right * hHalf))};
  frustum.top = {position, glm::normalize(glm::cross(right, farPos - up * vHalf))};
  frustum.bottom = {position, glm::normalize(glm::cross(farPos + up * vHalf, right))};
}
void Camera::Frame(const BoundingBox& bounds) {
  auto center = (bounds.min + bounds.max) * .5f;
  auto dimensions = bounds.max - bounds.min;
  auto radius = glm::length(dimensions) * .5f;
  float fovVertical = glm::radians(fov);
  float fovHorizontal = 2.0f * atan(tan(fovVertical * .5f) * aspectRatio);
  auto fovMin = glm::min(fovVertical, fovHorizontal);
  auto distanceFactor = 1.1f;
  float distance = (radius / tan(fovMin * .5f)) * distanceFactor;
  position = center - front * distance;
  farPlane = distance + radius;
  UpdateView();
  UpdateProjection();
}
bool Camera::OverlapsFrustum(const BoundingBox& bounds) const {
  return frustum.OverlapsFrustum(bounds);
}
} // namespace kuki
