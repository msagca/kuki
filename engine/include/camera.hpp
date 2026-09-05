#pragma once
#include <bounding_box.hpp>
#include <camera_type.hpp>
#include <debug_view.hpp>
#include <exposure.hpp>
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
  /// @brief How much light this camera gathers, and where that figure comes from.
  ///
  /// Exposure sits on the camera rather than beside the tone mapping curve because it is a property
  /// of the thing doing the looking: two cameras in one scene can reasonably disagree about it, and
  /// it travels with the scene the way a lens or a field of view does. The curve is the opposite
  /// sort of setting and stays where it is, in the config, because it describes the display someone
  /// is working at rather than anything about their scene.
  ///
  /// `exposure` is a compensation in stops and applies in both modes; the remaining three are read
  /// only in `ExposureMode::Physical`.
  ExposureMode exposureMode{ExposureMode::Manual};
  float exposure{DEFAULT_EXPOSURE};
  float aperture{DEFAULT_APERTURE};
  float shutterSpeed{DEFAULT_SHUTTER_SPEED};
  float sensitivity{DEFAULT_SENSITIVITY};
  /// @brief Which step of the shading this camera draws instead of the finished pixel, and what it
  /// draws the probes as.
  ///
  /// On the camera because a debug view is a way of looking, and a camera is the thing that looks.
  /// Putting them here buys something a global setting cannot: a scene may hold several cameras,
  /// each set to a different view, and switching between them switches what is being examined
  /// without disturbing either setting. Two cameras in one scene can disagree about this exactly as
  /// they can disagree about exposure, and for the same reason.
  ///
  /// Deliberately absent from `ToJson`, unlike every other field here. A curve or an exposure is
  /// part of the shot and belongs in the scene file; a debug view is a thing you are in the middle
  /// of looking at, and a scene that reopens showing a picture of a weight buffer looks broken
  /// rather than restored.
  LightingDebugView lightingDebugView{};
  ProbeDebugView probeDebugView{};
  auto Frame(const BoundingBox &, glm::vec3 = {0.f, 0.f, 0.f}, glm::vec3 = {-.26f, .52f, 0.f}, float = 1.1f) -> void;
  /// @brief Stops of scale the tone mapping pass should apply, whichever mode is in use.
  ///
  /// The single number both backends read, so that neither has to know a physical camera exists.
  auto GetExposureStops() const -> float;
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
