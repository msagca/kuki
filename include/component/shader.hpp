#pragma once
#include "component.hpp"
#include <string>
#include <variant>
#include <vector>
struct Shader : IComponent {
  unsigned int id{};
  std::string GetName() const override {
    return "Shader";
  }
  std::vector<Property> GetProperties() const override {
    // TODO: the editor should display a friendly name instead of a shader ID; also, a preview of the shader would be nice
    return {{"ID", id}};
  }
  void SetProperty(Property property) override {
    if (std::holds_alternative<unsigned int>(property.value))
      if (property.name == "ID")
        id = std::get<unsigned int>(property.value);
  }
};
