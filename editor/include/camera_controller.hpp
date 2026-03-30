#pragma once
#include <camera.hpp>
#include <controller_settings.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <input_manager.hpp>
namespace kuki {
class Application;
}
using namespace kuki;
/// @brief An FPS-style camera controller for the editor
class CameraController {
public:
  // TODO: make this script a component of an entity
  CameraController(Application &, EntityID);
  Camera camera{};
  bool mouseEnter{true};
  bool mouselook{false};
  auto Update(const float) -> void;
private:
  Application &app;
  ControllerSettings settings{};
  EntityID entityId;
  bool cameraMissing{true};
  glm::vec2 mouseLast;
  auto UpdatePosition(const float) -> bool;
  auto UpdateRotation() -> bool;
};
