#pragma once
#include "component.hpp"
#include <glm/ext/vector_float3.hpp>
#include <string>
#include <variant>
#include <vector>
struct Transform : IComponent {
  glm::vec3 position{};
  glm::vec3 rotation{}; // NOTE: these should be in radians and converted to degrees when displayed in the editor
  glm::vec3 scale{1.0f};
  int parent{-1}; // parent entity/asset index
  std::string GetName() const override {
    return "Transform";
  }
  std::vector<Property> GetProperties() const override {
    return {{"Position", position}, {"Rotation", rotation}, {"Scale", scale}, {"Parent", parent}};
  }
  void SetProperty(Property property) override {
    if (std::holds_alternative<glm::vec3>(property.value)) {
      auto& value = std::get<glm::vec3>(property.value);
      if (property.name == "Position")
        position = value;
      else if (property.name == "Rotation")
        rotation = value;
      else if (property.name == "Scale")
        scale = value;
    } else if (std::holds_alternative<int>(property.value)) {
      auto& value = std::get<int>(property.value);
      if (property.name == "Parent")
        parent = value;
    }
  }
};
