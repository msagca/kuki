#define GLM_ENABLE_EXPERIMENTAL
#include <bounding_box.hpp>
#include <camera.hpp>
#include <camera_type.hpp>
#include <cmath>
#include <component.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <glm/geometric.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/quaternion.hpp>
#include <transform.hpp>
namespace kuki {
auto Camera::Frame(const BoundingBox &bounds, float distanceFactor) -> void {
  const auto center = (bounds.min + bounds.max) * .5f;
  const auto dimensions = bounds.max - bounds.min;
  const auto radius = glm::length(dimensions) * .5f;
  const auto fovVertical = glm::radians(fov);
  const float fovHorizontal = 2.f * atan(tan(fovVertical * .5f) * aspectRatio);
  const auto fovMin = glm::min(fovVertical, fovHorizontal);
  const float distance = (radius / tan(fovMin * .5f)) * distanceFactor;
  auto pitch = glm::radians(-15.f);
  auto yaw = glm::radians(30.f);
  // capture the scene from an angle
  rotation = glm::yawPitchRoll(yaw, pitch, 0.f);
  UpdateBasis();
  position = center - forward * distance;
  if (distance + radius > farPlane)
    farPlane = distance + radius;
  UpdateTransform();
  UpdateFrustum();
  ++dirty;
}
auto Camera::GetTransform() const -> Transform {
  Transform transform;
  transform.position = position;
  transform.rotation = rotation;
  transform.local = local;
  return transform;
}
auto Camera::IntersectsFrustum(const BoundingBox &bounds) const -> bool {
  return frustum.InFrustum(bounds);
}
auto Camera::SetTransform(const Transform &transform) -> void {
  position = transform.position;
  rotation = transform.rotation;
  Update();
  ++dirty;
}
auto Camera::Update() -> void {
  UpdateBasis();
  UpdateTransform();
  UpdateFrustum();
}
inline auto Camera::UpdateBasis() -> void {
  auto R = glm::toMat4(rotation);
  right = glm::vec3(R[0]);
  up = glm::vec3(R[1]);
  forward = -glm::vec3(R[2]);
}
inline auto Camera::UpdateFrustum() -> void {
  auto vp = transform.projection * transform.view;
  frustum.left = {vp[3] + vp[0]};
  frustum.right = {vp[3] - vp[0]};
  frustum.bottom = {vp[3] + vp[1]};
  frustum.top = {vp[3] - vp[1]};
  frustum.near = {vp[3] + vp[2]};
  frustum.far = {vp[3] - vp[2]};
}
inline auto Camera::UpdateTransform() -> void {
  auto T = glm::translate(glm::mat4(1.f), position);
  auto R = glm::toMat4(rotation);
  local = T * R;
  transform.view = glm::lookAt(position, position + forward, up);
  if (type == CameraType::Perspective)
    transform.projection = glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane);
  else {
    auto left = -orthoSize * aspectRatio;
    auto bottom = -orthoSize;
    transform.projection = glm::ortho(left, -left, bottom, -bottom, nearPlane, farPlane);
  }
}
} // namespace kuki
