#pragma once
#include <component_types.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <input_manager.hpp>
class CameraController {
private:
  Camera& camera;
  InputManager& inputManager;
  float moveSpeed;
  float mouseSensitivity;
  void UpdateRotation(glm::vec2);
  void UpdatePosition();
  void UpdateDirection();
  void UpdateView();
  void UpdateProjection();
public:
  CameraController(Camera&, InputManager&);
  void SetCamera(Camera&);
  void SetPosition(glm::vec3);
  void SetAspect(float);
  void SetFOV(float);
  void Update();
};
