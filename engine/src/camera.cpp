#define GLM_ENABLE_EXPERIMENTAL
#include <bounding_box.hpp>
#include <camera.hpp>
#include <camera_type.hpp>
#include <cmath>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/quaternion_float.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtx/quaternion.hpp>
#include <transform.hpp>
namespace kuki {
auto Camera::Frame(const BoundingBox &bounds, glm::vec3 center, glm::vec3 angle, float distanceFactor) -> void {
  const auto dimensions = bounds.max - bounds.min;
  const auto radius = glm::length(dimensions) * .5f;
  const auto fovVertical = glm::radians(fov);
  const float fovHorizontal = 2.f * atan(tan(fovVertical * .5f) * aspectRatio);
  const auto fovMin = glm::min(fovVertical, fovHorizontal);
  const float distance = (radius / tan(fovMin * .5f)) * distanceFactor;
  rotation = glm::quat(angle);
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
  transform.world = transform.local;
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
  const auto row0 = glm::row(vp, 0);
  const auto row1 = glm::row(vp, 1);
  const auto row2 = glm::row(vp, 2);
  const auto row3 = glm::row(vp, 3);
  frustum.left = {row3 + row0};
  frustum.right = {row3 - row0};
  frustum.bottom = {row3 + row1};
  frustum.top = {row3 - row1};
  frustum.near = {row3 + row2};
  frustum.far = {row3 - row2};
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
