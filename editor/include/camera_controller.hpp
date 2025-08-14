#pragma once
#include <component/camera.hpp>
#include <component/script.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <input_manager.hpp>
using namespace kuki;
class CameraController final : public IScript {
private:
  Camera camera;
  float moveSpeed{5.0f};
  float mouseSensitivity{.001f};
  bool movementEnabled{true};
  bool rotationEnabled{true};
  void UpdatePosition(float);
  void UpdateRotation(glm::vec2);
public:
  using IScript::IScript;
  void Update(float) override;
  void ToggleMovement(bool);
  void ToggleRotation(bool);
  Camera* GetCamera();
};
