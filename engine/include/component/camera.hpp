#pragma once
#include "component.hpp"
#include <engine_export.h>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>
#include <nlohmann/json.hpp>
#include <vector>
struct ENGINE_API Camera : IComponent {
  CameraType type{CameraType::Perspective};
  glm::mat4 view{};
  glm::mat4 projection{};
  glm::vec3 position{.0f, 3.0f, 5.0f};
  glm::vec3 front{.0f, .0f, -1.0f};
  glm::vec3 up{.0f, 1.0f, .0f};
  glm::vec3 right{1.0f, .0f, .0f};
  float pitch{-30.0f};
  float yaw{-90.0f};
  float fov{45.0f};
  float aspectRatio{1.0f};
  float nearPlane{.1f};
  float farPlane{100.0f};
  float orthoSize{10.0f};
  const std::string GetName() const override;
  std::vector<Property> GetProperties() const override;
  void SetProperty(Property) override;
  void SetAspectRatio(float);
  void UpdateDirection();
  void UpdateView();
  void UpdateProjection();
};
