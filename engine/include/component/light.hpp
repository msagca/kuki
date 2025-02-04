#pragma once
#include "component.hpp"
#include <glm/ext/vector_float3.hpp>
#include <string>
#include <variant>
#include <vector>
struct Light : IComponent {
  LightType type{LightType::Directional};
  glm::vec3 vector{.2f, 1.0f, .3f};
  glm::vec3 ambient{.2f};
  glm::vec3 diffuse{.5f};
  glm::vec3 specular{1.0f};
  // attenuation terms (for point light)
  float constant{1.0f};
  float linear{.09f};
  float quadratic{.032f};
  std::string GetName() const override {
    return "Light";
  }
  std::vector<Property> GetProperties() const override {
    return {{"Type", type}, {"Vector", vector}, {"Ambient", ambient}, {"Diffuse", diffuse}, {"Specular", specular}, {"Constant", constant}, {"Linear", linear}, {"Quadratic", quadratic}};
  }
  void SetProperty(Property property) override {
    if (std::holds_alternative<glm::vec3>(property.value)) {
      auto& value = std::get<glm::vec3>(property.value);
      if (property.name == "Vector")
        vector = value;
      else if (property.name == "Ambient")
        ambient = value;
      else if (property.name == "Diffuse")
        diffuse = value;
      else if (property.name == "Specular")
        specular = value;
    } else if (std::holds_alternative<float>(property.value)) {
      auto& value = std::get<float>(property.value);
      if (property.name == "Constant")
        constant = value;
      else if (property.name == "Linear")
        linear = value;
      else if (property.name == "Quadratic")
        quadratic = value;
    } else if (std::holds_alternative<LightType>(property.value))
      if (property.name == "Type")
        type = std::get<LightType>(property.value);
  }
};
