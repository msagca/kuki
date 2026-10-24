#pragma once
#include <camera.hpp>
#include <controller_settings.hpp>
#include <glm/ext/matrix_float4x4.hpp>
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
  auto GetProjection() const -> const glm::mat4 &;
  auto GetType() const -> const kuki::CameraType &;
  auto GetView() const -> const glm::mat4 &;
  /// @brief Registers the keys and mouse buttons it flies by.
  auto Start() -> void override;
  /// @brief Unregisters them: the actions capture `this`.
  auto Shutdown() -> void override;
  /// @brief Which entity's `Camera` it flies. Pointed again whenever a scene is loaded.
  auto SetEntity(const kuki::EntityID) -> void;
  auto GetEntity() const -> kuki::EntityID;
  /// @brief Applies the frame's input, then syncs with the entity's `Camera` component.
  ///
  /// The sync is deliberately lopsided. Outwards it writes only the placement -- position, rotation
  /// and orthographic size -- and only when this controller's `dirty` counter is the newer of the
  /// two, so a position set in the properties panel or by `AlignView` is not undone. Inwards it
  /// copies the whole component every frame, because the component is the camera and this is a
  /// cache of it: anything the placement list does not mention belongs to somebody else and must
  /// not be written back from here.
  ///
  /// The comparison is on a wrapping counter, so the sync direction briefly inverts if `dirty` wraps around.
  auto Update(const float) -> void override;
private:
  kuki::EntityID entity{};
  kuki::Camera camera{};
  ControllerSettings settings{};
  bool cameraMissing{true};
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
  auto UpdateZoom(kuki::Application &) -> bool;
};
