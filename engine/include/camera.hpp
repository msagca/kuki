#pragma once
#include <bounding_box.hpp>
#include <camera_type.hpp>
#include <frustum.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>
#include <kuki_engine_export.h>
#include <plane.hpp>
#include <transform.hpp>
namespace kuki {
struct CameraTransform {
  glm::mat4 view{};
  glm::mat4 projection{};
};
struct KUKI_ENGINE_API Camera {
  CameraType type{CameraType::Perspective};
  glm::vec3 position{};
  glm::quat rotation{1.f, .0f, .0f, .0f};
  glm::vec3 forward{.0f, .0f, -1.f};
  glm::vec3 up{.0f, 1.f, .0f};
  glm::vec3 right{1.f, .0f, .0f};
  glm::mat4 local{1.f};
  CameraTransform transform;
  mutable GenCount dirty{};
  Frustum frustum{};
  float fov{45.f};
  float aspectRatio{1.f};
  float nearPlane{.1f};
  float farPlane{100.f};
  float orthoSize{10.f};
  auto Frame(const BoundingBox &, glm::vec3 = {0.f, 0.f, 0.f}, glm::vec3 = {-.26f, .52f, 0.f}, float = 1.1f) -> void;
  auto GetTransform() const -> Transform;
  auto IntersectsFrustum(const BoundingBox &) const -> bool;
  auto SetTransform(const Transform &) -> void;
  auto Update() -> void;
private:
  auto UpdateBasis() -> void;
  auto UpdateFrustum() -> void;
  auto UpdateTransform() -> void;
};
} // namespace kuki
