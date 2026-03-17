#pragma once
#include <camera.hpp>
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
  CameraController(Application &, EntityID);
  Camera camera{};
  float mouseSensitivity{.001f};
  bool mouselook{true};
  auto Update(const float) -> void;
private:
  Application &app;
  EntityID entityId{};
  auto UpdatePosition(const float) -> bool;
  auto UpdateRotation(glm::vec2) -> bool;
};
