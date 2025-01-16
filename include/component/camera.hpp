#pragma once
#include "component.hpp"
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <variant>
#include <vector>
struct Camera : IComponent {
  // TODO: add the orthogonal projection option
  glm::mat4 view{};
  glm::mat4 projection{};
  glm::vec3 position{.0f, .0f, 3.0f};
  glm::vec3 front{.0f, .0f, -1.0f};
  glm::vec3 up{.0f, 1.0f, .0f};
  glm::vec3 right{1.0f, .0f, .0f};
  float pitch{};
  float yaw{-90.0f};
  float fov{45.0f};
  float aspect{1.0f};
  float near{.1f};
  float far{100.0f};
  std::string GetName() const override {
    return "Camera";
  }
  std::vector<Property> GetProperties() const override {
    return {{"Position", position}, {"Pitch", pitch}, {"Yaw", yaw}, {"FOV", fov}, {"Aspect", aspect}, {"Near", near}, {"Far", far}};
  }
  void SetProperty(Property property) override {
    if (std::holds_alternative<float>(property.value)) {
      auto& value = std::get<float>(property.value);
      if (property.name == "Pitch")
        pitch = value;
      else if (property.name == "Yaw")
        yaw = value;
      else if (property.name == "FOV")
        fov = value;
      else if (property.name == "Aspect")
        aspect = value;
      else if (property.name == "Near")
        near = value;
      else if (property.name == "Far")
        far = value;
    } else if (std::holds_alternative<glm::vec3>(property.value))
      if (property.name == "Position")
        position = std::get<glm::vec3>(property.value);
  }
};
