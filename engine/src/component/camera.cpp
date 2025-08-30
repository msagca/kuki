#include <cmath>
#include <component/camera.hpp>
#include <component/component.hpp>
#include <component/mesh.hpp>
#include <component/transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_float3x3.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/vector_float3.hpp>
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
  auto extents = bounds.max - center;
  const Plane planes[6] = {near, far, right, left, top, bottom};
  for (const auto& plane : planes)
    if (!plane.OnPlane(center, extents))
      return false;
  return true;
}
Transform Camera::GetTransform() const {
  Transform transform;
  transform.position = position;
  glm::mat3 rotation(right, up, forward);
  transform.rotation = glm::quat_cast(rotation);
  return transform;
}
void Camera::SetTransform(const Transform& transform) {
  position = transform.position;
  auto rotation = glm::mat3_cast(transform.rotation);
  right = rotation[0];
  up = rotation[1];
  forward = rotation[2];
  auto euler = glm::eulerAngles(transform.rotation);
  pitch = euler.x;
  yaw = euler.y;
  // FIXME: if a CameraController is present in the scene, calling Update() here will cause issues
}
void Camera::Update() {
  UpdateDirection();
  UpdateView();
  UpdateProjection();
  UpdateFrustum();
}
void Camera::UpdateDirection() {
  static constexpr auto PITCH_LIMIT = 89.0f;
  static constexpr auto WORLD_UP = glm::vec3(.0f, 1.0f, .0f);
  pitch = glm::clamp(pitch, -PITCH_LIMIT, PITCH_LIMIT);
  forward.x = cos(yaw) * cos(pitch);
  forward.y = sin(pitch);
  forward.z = sin(yaw) * cos(pitch);
  forward = glm::normalize(forward);
  right = glm::normalize(glm::cross(forward, WORLD_UP));
  up = glm::normalize(glm::cross(right, forward));
}
void Camera::UpdateView() {
  view = glm::lookAt(position, position + forward, up);
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
  const auto farPos = farPlane * forward;
  frustum.near = {position + nearPlane * forward, forward};
  frustum.far = {position + farPos, -forward};
  frustum.right = {position, glm::normalize(glm::cross(farPos - right * hHalf, up))};
  frustum.left = {position, glm::normalize(glm::cross(up, farPos + right * hHalf))};
  frustum.top = {position, glm::normalize(glm::cross(right, farPos - up * vHalf))};
  frustum.bottom = {position, glm::normalize(glm::cross(farPos + up * vHalf, right))};
}
void Camera::Frame(const BoundingBox& bounds) {
  static constexpr auto DISTANCE_FACTOR = 1.1f;
  const auto center = (bounds.min + bounds.max) * .5f;
  const auto dimensions = bounds.max - bounds.min;
  const auto radius = glm::length(dimensions) * .5f;
  const auto fovVertical = glm::radians(fov);
  const float fovHorizontal = 2.0f * atan(tan(fovVertical * .5f) * aspectRatio);
  const auto fovMin = glm::min(fovVertical, fovHorizontal);
  const float distance = (radius / tan(fovMin * .5f)) * DISTANCE_FACTOR;
  position = center - forward * distance;
  farPlane = distance + radius;
  Update();
}
bool Camera::IntersectsFrustum(const BoundingBox& bounds) const {
  return frustum.OverlapsFrustum(bounds);
}
} // namespace kuki
