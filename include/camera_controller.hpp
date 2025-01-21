#pragma once
#include "component/camera.hpp"
#include <entity_manager.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
class CameraController {
private:
  Camera camera;
  EntityManager& entityManager;
  float moveSpeed;
  float mouseSensitivity;
  void UpdateRotation(glm::vec2);
  void UpdatePosition();
  void UpdateDirection();
  void UpdateView();
  void UpdateProjection();
public:
  CameraController(EntityManager&);
  glm::vec3 GetPosition() const;
  glm::vec3 GetFront() const;
  float GetFOV() const;
  float GetFar() const;
  glm::mat4 GetView() const;
  glm::mat4 GetProjection() const;
  void SetAspect(float);
  void Update();
};
