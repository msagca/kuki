#pragma once
#include <camera.hpp>
#include <controller_settings.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <input_manager.hpp>
#include <script.hpp>
namespace kuki {
class Application;
}
class CameraController final : public kuki::Script {
public:
  CameraController();
  ~CameraController() override;
  auto CloneTo(kuki::EntityManager &, const kuki::EntityID) const -> void override;
  auto Display() const -> void override;
  auto GetName() const -> std::string override;
  auto GetProjection() const -> const glm::mat4 &;
  auto GetType() const -> const kuki::CameraType &;
  auto GetView() const -> const glm::mat4 &;
  auto Start(kuki::Application &) -> void override;
  auto Update(kuki::Application &) -> void override;
private:
  kuki::Camera camera{};
  // NOTE: mutable so `Display() const` can expose these as editable sliders in the Inspector.
  mutable ControllerSettings settings{};
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
  kuki::Application *app{};
  kuki::InputManager::ActionID rmbPressActionId{kuki::InputManager::InvalidActionID};
  kuki::InputManager::ActionID rmbReleaseActionId{kuki::InputManager::InvalidActionID};
  kuki::InputManager::ActionID lmbPressActionId{kuki::InputManager::InvalidActionID};
  kuki::InputManager::ActionID lmbReleaseActionId{kuki::InputManager::InvalidActionID};
  kuki::InputManager::ActionID mmbPressActionId{kuki::InputManager::InvalidActionID};
  kuki::InputManager::ActionID mmbReleaseActionId{kuki::InputManager::InvalidActionID};
  auto UpdateOrbit(kuki::Application &) -> bool;
  auto UpdatePan(kuki::Application &) -> bool;
  auto UpdatePosition(kuki::Application &) -> bool;
  auto UpdateRotation(kuki::Application &) -> bool;
  auto UpdateZoom(kuki::Application &) -> bool;
};
