#pragma once
#include "component.hpp"
#include <string>
#include <variant>
#include <vector>
struct Material : IComponent {
  unsigned int base{};
  unsigned int normal{};
  unsigned int orm{};
  unsigned int metalness{};
  unsigned int occlusion{};
  unsigned int roughness{};
  std::string GetName() const override {
    return "Material";
  }
  std::vector<Property> GetProperties() const override {
    return {{"Base", base}, {"Normal", normal}, {"ORM", orm}, {"Metalness", metalness}, {"Occlusion", occlusion}, {"Roughness", roughness}};
  }
  void SetProperty(Property property) override {
    if (std::holds_alternative<unsigned int>(property.value)) {
      auto& value = std::get<float>(property.value);
      if (property.name == "Base")
        base = value;
      else if (property.name == "Normal")
        normal = value;
      else if (property.name == "ORM")
        orm = value;
      else if (property.name == "Metalness")
        metalness = value;
      else if (property.name == "Occlusion")
        occlusion = value;
      else if (property.name == "Roughness")
        roughness = value;
    }
  }
};
