#pragma once
#include "component.hpp"
#include "mesh.hpp"
#include "transform.hpp"
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/trigonometric.hpp>
#include <kuki_engine_export.h>
namespace kuki {
struct KUKI_ENGINE_API Plane {
  glm::vec3 point{};
  glm::vec3 normal{};
  bool OnPlane(const glm::vec3&, const glm::vec3&) const;
  float SignedDistance(const glm::vec3&) const;
};
struct KUKI_ENGINE_API Frustum {
  Plane top{};
  Plane bottom{};
  Plane right{};
  Plane left{};
  Plane far{};
  Plane near{};
  bool OverlapsFrustum(const BoundingBox&) const;
};
struct KUKI_ENGINE_API Camera final : public IComponent {
  Camera()
    : IComponent(std::in_place_type<Camera>) {}
  CameraType type{CameraType::Perspective};
  glm::mat4 view{};
  glm::mat4 projection{};
  glm::vec3 position{-3.0f, 1.5f, 3.0f};
  glm::vec3 forward{.0f, .0f, -1.0f};
  glm::vec3 up{.0f, 1.0f, .0f};
  glm::vec3 right{1.0f, .0f, .0f};
  Frustum frustum{};
  float pitch{glm::radians(-15.0f)};
  float yaw{glm::radians(-45.0f)};
  float fov{45.0f};
  float aspectRatio{1.0f};
  float nearPlane{.1f};
  float farPlane{100.0f};
  float orthoSize{2.0f};
  Transform GetTransform() const;
  void SetTransform(const Transform&);
  void Update();
  void UpdateDirection();
  void UpdateView();
  void UpdateProjection();
  void UpdateFrustum();
  /// @brief Position the camera to fully capture the target within the given bounds
  void Frame(const BoundingBox&);
  bool IntersectsFrustum(const BoundingBox&) const;
};
} // namespace kuki
