#include <bounding_box.hpp>
#include <camera.hpp>
#include <camera_type.hpp>
#include <algorithm>
#include <cmath>
#include <exposure.hpp>
#include <frustum.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/quaternion_float.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtx/quaternion.hpp>
#include <transform.hpp>
namespace kuki {
namespace {
  auto ComputeProjection(const Camera &camera) -> glm::mat4 {
    if (camera.type == CameraType::Perspective)
      return glm::perspective(glm::radians(camera.fov), camera.aspectRatio, camera.nearPlane, camera.farPlane);
    const auto left = -camera.orthoSize * camera.aspectRatio;
    const auto bottom = -camera.orthoSize;
    return glm::ortho(left, -left, bottom, -bottom, camera.nearPlane, camera.farPlane);
  }
  auto ComputeFrustum(const glm::mat4 &viewProjection) -> Frustum {
    const auto row0 = glm::row(viewProjection, 0);
    const auto row1 = glm::row(viewProjection, 1);
    const auto row2 = glm::row(viewProjection, 2);
    const auto row3 = glm::row(viewProjection, 3);
    Frustum frustum;
    frustum.left = {row3 + row0};
    frustum.right = {row3 - row0};
    frustum.bottom = {row3 + row1};
    frustum.top = {row3 - row1};
    frustum.near = {row3 + row2};
    frustum.far = {row3 - row2};
    return frustum;
  }
} // namespace
auto Frame(const Camera &camera, const BoundingBox &bounds, glm::vec3 center, glm::vec3 angle, float distanceFactor) -> CameraFraming {
  const auto dimensions = bounds.max - bounds.min;
  const auto radius = glm::length(dimensions) * .5f;
  const auto fovVertical = glm::radians(camera.fov);
  const float fovHorizontal = 2.f * atan(tan(fovVertical * .5f) * camera.aspectRatio);
  const auto fovMin = glm::min(fovVertical, fovHorizontal);
  const float distance = (radius / tan(fovMin * .5f)) * distanceFactor;
  CameraFraming framing;
  framing.rotation = glm::quat(angle);
  framing.position = center - WorldForward(glm::toMat4(framing.rotation)) * distance;
  framing.farPlane = std::max(camera.farPlane, distance + radius);
  return framing;
}
auto ComputeCameraTransform(const Camera &camera, const glm::mat4 &world) -> CameraTransform {
  CameraTransform transform;
  transform.view = ViewFrom(world);
  transform.projection = ComputeProjection(camera);
  transform.position = WorldPosition(world);
  transform.frustum = ComputeFrustum(transform.projection * transform.view);
  return transform;
}
auto GetExposureStops(const Camera &camera) -> float {
  return ComputeExposureStops(camera.exposureMode, camera.exposure, camera.aperture, camera.shutterSpeed, camera.sensitivity);
}
auto IntersectsFrustum(const Camera &camera, const BoundingBox &bounds) -> bool {
  return camera.transform.frustum.InFrustum(bounds);
}
} // namespace kuki
