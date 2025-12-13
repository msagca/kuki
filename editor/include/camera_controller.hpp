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
private:
  Application& app;
  ID entityId{};
  bool UpdatePosition(float);
  bool UpdateRotation(glm::vec2);
public:
  CameraController(Application&, ID);
  Camera camera{};
  float mouseSensitivity{.001f};
  bool mouselook{true};
  void Update(float);
};
