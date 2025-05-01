#pragma once
#include <application.hpp>
#include <component/camera.hpp>
#include <component/script.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <input_manager.hpp>
class CameraController final : public kuki::IScript {
private:
  kuki::Camera camera;
  float moveSpeed{5.0f};
  float mouseSensitivity{.001f};
  bool movementEnabled{true};
  bool rotationEnabled{true};
  void UpdatePosition(float);
  void UpdateRotation(glm::vec2);
public:
  using kuki::IScript::IScript;
  const std::string GetName() const override;
  std::vector<kuki::Property> GetProperties() const override;
  void SetProperty(kuki::Property) override;
  void Update(float) override;
  void ToggleMovement(bool);
  void ToggleRotation(bool);
  kuki::Camera* GetCamera();
};
