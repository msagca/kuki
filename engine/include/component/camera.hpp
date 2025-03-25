#pragma once
#include "component.hpp"
#include "component/mesh.hpp"
#include <engine_export.h>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>
#include <vector>
struct ENGINE_API Plane {
  glm::vec3 point{.0f};
  glm::vec3 normal{.0f};
  bool OnPlane(const glm::vec3&, const glm::vec3&) const;
  float SignedDistance(const glm::vec3&) const;
};
struct ENGINE_API Frustum {
  Plane top;
  Plane bottom;
  Plane right;
  Plane left;
  Plane far;
  Plane near;
  bool OverlapsFrustum(const BoundingBox&) const;
};
struct ENGINE_API Camera final : IComponent {
  CameraType type{CameraType::Perspective};
  glm::mat4 view{};
  glm::mat4 projection{};
  glm::vec3 position{.0f, 3.0f, 5.0f};
  glm::vec3 front{.0f, .0f, -1.0f};
  glm::vec3 up{.0f, 1.0f, .0f};
  glm::vec3 right{1.0f, .0f, .0f};
  Frustum frustum;
  float pitch{-15.0f};
  float yaw{-90.0f};
  float fov{45.0f};
  float aspectRatio{1.0f};
  float nearPlane{.1f};
  float farPlane{100.0f};
  float orthoSize{10.0f};
  const std::string GetName() const override;
  std::vector<Property> GetProperties() const override;
  void SetProperty(Property) override;
  void UpdateDirection();
  void UpdateView();
  void UpdateProjection();
  void UpdateFrustum();
  /// @brief Position the camera to fully capture the target within the given bounds
  void Frame(const BoundingBox&);
  bool OverlapsFrustum(const BoundingBox&) const;
};
