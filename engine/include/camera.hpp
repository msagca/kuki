#pragma once
#include <component.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/trigonometric.hpp>
#include <kuki_engine_export.h>
#include <mesh.hpp>
#include <transform.hpp>
namespace kuki {
struct KUKI_ENGINE_API Plane {
  glm::vec3 point{};
  glm::vec3 normal{.0f, 1.0f, .0f};
  Plane();
  Plane(const glm::vec3&, const glm::vec3&);
  /// @brief Construct a plane from a given equation `ax + by + cz + d = 0`
  Plane(const glm::vec4&);
  /// @brief Determines whether the given bounding box is on the positive side of the plane
  bool OnPositiveSide(const BoundingBox&) const;
  float SignedDistance(const glm::vec3&) const;
};
struct KUKI_ENGINE_API Frustum {
  Plane top{};
  Plane bottom{};
  Plane right{};
  Plane left{};
  Plane far{};
  Plane near{};
  bool InFrustum(const BoundingBox&) const;
};
struct KUKI_ENGINE_API CameraTransform {
  glm::mat4 view{};
  glm::mat4 projection{};
};
struct KUKI_ENGINE_API Camera final : public IComponent {
  Camera();
  CameraType type{CameraType::Perspective};
  glm::vec3 position{};
  glm::quat rotation{};
  glm::vec3 forward{.0f, .0f, -1.0f};
  glm::vec3 up{.0f, 1.0f, .0f};
  glm::vec3 right{1.0f, .0f, .0f};
  /// @brief Local transform matrix of the camera
  glm::mat4 local{1.0f};
  CameraTransform transform;
  // TODO: turn the following dirty flags into a bitfield
  mutable bool positionDirty{true};
  mutable bool rotationDirty{true};
  mutable bool settingsDirty{true};
  /// @brief Indicates if uniform buffer data needs to be resend
  mutable bool uboDirty{true};
  Frustum frustum{};
  float fov{45.0f};
  float aspectRatio{1.0f};
  float nearPlane{.1f};
  float farPlane{100.0f};
  float orthoSize{2.0f};
  Transform GetTransform() const;
  void SetTransform(const Transform&);
  void Update();
  /// @brief Position the camera to fully capture the given subject
  /// @param bounds `BoundingBox` of the subject's mesh
  /// @param distanceFactor Zoom out amount in `float`
  void Frame(const BoundingBox&, float = 1.1f);
  bool IntersectsFrustum(const BoundingBox&) const;
private:
  void UpdateBasis();
  void UpdateTransform();
  void UpdateFrustum();
  void ClearFlags();
};
} // namespace kuki
