#pragma once
#include <component/camera.hpp>
#include <entity_manager.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <input_manager.hpp>
class CameraController {
private:
  Camera camera;
  Camera* cameraPtr = nullptr;
  InputManager& inputManager;
  float moveSpeed = 2.5f;
  float mouseSensitivity = .1f;
  void UpdateRotation(glm::vec2);
  void UpdatePosition(float);
  void UpdateDirection();
  void UpdateView();
  void UpdateProjection();
public:
  CameraController(InputManager&);
  Camera& GetCamera();
  void SetCamera(Camera*);
  glm::vec3 GetPosition() const;
  glm::vec3 GetFront() const;
  float GetFOV() const;
  float GetFar() const;
  glm::mat4 GetView() const;
  glm::mat4 GetProjection() const;
  void SetAspect(float);
  void Update(float);
};
