#pragma once
#include <bounding_box.hpp>
#include <camera_type.hpp>
#include <debug_view.hpp>
#include <exposure.hpp>
#include <frustum.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/quaternion_float.hpp>
#include <glm/ext/vector_float3.hpp>
#include <kuki_engine_export.h>
#include <plane.hpp>
namespace kuki {
/// @brief What a camera sees from where its entity is, worked out each frame by `RenderingSystem`.
///
/// Derived, and only read: the camera's place is its entity's `Transform`, and these follow from
/// that place's `WorldMatrix` and the lens fields beside them. The camera used to keep a position and
/// rotation of its own that nothing synced with the entity's, so a camera had two places, one of
/// them ignored, and a game moving it had to bump a counter and sometimes call `Update` for the
/// change to be seen. Now a game moves the entity, as it moves anything else.
///
/// Worked out after the transform update and before anything is drawn, so a game reading these in
/// its own `Update` reads where the camera was last frame.
struct CameraTransform {
  glm::mat4 view{1.f};
  glm::mat4 projection{1.f};
  /// Where the camera is in the world.
  glm::vec3 position{};
  Frustum frustum{};
};
/// @brief A lens, placed by its entity's `Transform`: it looks down the entity's -Z.
///
/// Move a camera as anything else is moved -- write the transform and `MarkTransformDirty` -- and
/// read what it sees from `transform`. Only `aspectRatio` is the engine's to write, from the size
/// of what it draws into.
struct Camera {
  CameraType type{CameraType::Perspective};
  CameraTransform transform;
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
};
/// @brief Where a camera goes to see all of a box, and how far it must see to.
struct CameraFraming {
  glm::vec3 position{};
  glm::quat rotation{1.f, .0f, .0f, .0f};
  /// The camera's far plane, pushed out if the box's far side was beyond it.
  float farPlane{};
};
/// @brief Aims a camera from `angle` (Euler, radians) at `center`, backed off until a box's bounding
/// sphere fills the narrower of its two fields of view, times `distanceFactor`.
KUKI_ENGINE_API auto Frame(const Camera &, const BoundingBox &, glm::vec3 center = {0.f, 0.f, 0.f}, glm::vec3 angle = {-.26f, .52f, 0.f}, float distanceFactor = 1.1f) -> CameraFraming;
/// @brief The view, projection, place and frustum of a camera placed by a world matrix.
KUKI_ENGINE_API auto ComputeCameraTransform(const Camera &, const glm::mat4 &world) -> CameraTransform;
/// @brief Stops of scale the tone mapping pass should apply, whichever mode is in use.
///
/// The single number both backends read, so that neither has to know a physical camera exists.
KUKI_ENGINE_API auto GetExposureStops(const Camera &) -> float;
/// @brief Whether a box is at least partly inside what the camera saw when last worked out.
KUKI_ENGINE_API auto IntersectsFrustum(const Camera &, const BoundingBox &) -> bool;
} // namespace kuki
