#pragma once
#include <camera.hpp>
#include <controller_settings.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/quaternion_float.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <id.hpp>
#include <input_manager.hpp>
#include <system.hpp>
namespace kuki {
class Application;
}
/// @brief Flies the editor's camera: moves, turns, orbits the selection and pans.
///
/// The editor's own, and nothing a scene holds. It was a script on the camera's entity, which made
/// it something a scene could save, copy, paste and be missing, and it had to say it ran only while
/// the game was stopped so that it let go of the keyboard when one was played. Now it is one of the
/// editor's systems, and finds its camera by the entity it is pointed at.
///
/// A system and not something the editor steps from its own `Update`, because that runs after the
/// frame has been drawn: the camera would be where the mouse put it a frame ago. Listed ahead of
/// `RenderingSystem`, it moves the camera before the frame that shows it.
class CameraController final : public kuki::System {
public:
  explicit CameraController(kuki::Application &);
  /// @brief The settings, as sliders, for the editor's settings panel.
  auto Display() -> void;
  /// @brief What the camera it flies saw when last worked out, for the gizmo. Identity, and a
  /// perspective, when it flies none.
  auto GetProjection() const -> glm::mat4;
  auto GetType() const -> kuki::CameraType;
  auto GetView() const -> glm::mat4;
  /// @brief Registers the keys and mouse buttons it flies by.
  auto Start() -> void override;
  /// @brief Unregisters them: the actions capture `this`.
  auto Shutdown() -> void override;
  /// @brief Which entity's `Camera` it flies. Pointed again whenever a scene is loaded.
  auto SetEntity(const kuki::EntityID) -> void;
  auto GetEntity() const -> kuki::EntityID;
  /// @brief Applies the frame's input to the entity's `Transform`, which is where its camera is.
  ///
  /// Read from the transform at the start and written back only if the input moved it, so a place
  /// set anywhere else -- the properties panel, the gizmo -- is where the next movement starts from.
  /// It kept a copy of the whole camera and synced the two by a counter each side bumped, which had
  /// to be told field by field which side owned what; with the camera's place on its entity there
  /// is one copy of it and nothing to arbitrate.
  ///
  /// The transform is the entity's place relative to its parent, so a camera hung under something
  /// turned flies in that thing's axes. The editor's own camera has no parent.
  auto Update(const float) -> void override;
private:
  kuki::EntityID entity{};
  /// Where the camera is and which way it faces, read from its transform at the start of `Update`
  /// and what the input works on.
  glm::vec3 position{};
  glm::quat rotation{1.f, .0f, .0f, .0f};
  ControllerSettings settings{};
  bool mouseEnter{true};
  bool mouselook{false};
  bool orbiting{false};
  bool panning{false};
  glm::vec2 mouseLast{.0f};
  glm::vec3 orbitPivot{};
  float orbitDistance{};
  float pitch{};
  float yaw{};
  kuki::InputManager::ActionID rmbPressActionId{kuki::InputManager::InvalidActionID};
  kuki::InputManager::ActionID rmbReleaseActionId{kuki::InputManager::InvalidActionID};
  kuki::InputManager::ActionID lmbPressActionId{kuki::InputManager::InvalidActionID};
  kuki::InputManager::ActionID lmbReleaseActionId{kuki::InputManager::InvalidActionID};
  kuki::InputManager::ActionID mmbPressActionId{kuki::InputManager::InvalidActionID};
  kuki::InputManager::ActionID mmbReleaseActionId{kuki::InputManager::InvalidActionID};
  auto Forward() const -> glm::vec3;
  auto Right() const -> glm::vec3;
  auto Up() const -> glm::vec3;
  auto UpdateOrbit(kuki::Application &) -> bool;
  auto UpdatePan(kuki::Application &) -> bool;
  /// @brief Applies movement input for the frame, ramping the boost and precision multipliers over time.
  ///
  /// The ramp is smoothstepped rather than linear, which gives acceleration a less abrupt, more vehicle-like feel.
  ///
  /// @return True when the camera moved.
  auto UpdatePosition(kuki::Application &) -> bool;
  /// @brief Turns mouse delta into yaw and pitch, clamping pitch to just under the poles.
  ///
  /// Both axes are inverted on the way in: Y because the window origin is the northwest corner, X because positive rotation is counter-clockwise when looking along the axis.
  ///
  /// @return True when the camera rotated.
  auto UpdateRotation(kuki::Application &) -> bool;
  /// @brief Moves along the view under a perspective, or narrows the view under an orthographic
  /// projection by writing the camera's size.
  ///
  /// @return True when the camera moved; a narrowed view is not a move.
  auto UpdateZoom(kuki::Application &, kuki::Camera &) -> bool;
};
