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
using namespace kuki;
/// @brief An FPS-style camera controller
class CameraController final : public Script {
public:
  CameraController();
  auto CloneTo(EntityManager &, const EntityID) const -> void override;
  auto Display() const -> void override;
  auto GetProjection() const -> const glm::mat4 &;
  auto GetType() const -> const CameraType &;
  auto GetView() const -> const glm::mat4 &;
  auto Start(Application &) -> void override;
  auto Update(Application &) -> void override;
private:
  Camera camera{};
  ControllerSettings settings{};
  bool cameraMissing{true};
  bool mouseEnter{true};
  bool mouselook{false};
  glm::vec2 mouseLast{.0f};
  auto UpdatePosition(Application &) -> bool;
  auto UpdateRotation(Application &) -> bool;
};
