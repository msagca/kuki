#pragma once
#include "component.hpp"
#include "material.hpp"
#include "shader.hpp"
#include <string>
#include <vector>
struct MeshRenderer : IComponent {
  Shader shader{};
  Material material{};
  std::string GetName() const override {
    return "MeshRenderer";
  }
  std::vector<Property> GetProperties() const override {
    auto properties = shader.GetProperties();
    auto materialProperties = material.GetProperties();
    properties.insert(properties.end(), materialProperties.begin(), materialProperties.end());
    return properties;
  }
  void SetProperty(Property property) override {
    // FIXME: if two subcomponents share a property with the same name, both will be overwritten
    shader.SetProperty(property);
    material.SetProperty(property);
  }
};
