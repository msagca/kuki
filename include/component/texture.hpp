#pragma once
#include "component.hpp"
#include <string>
#include <variant>
#include <vector>
struct Texture : IComponent {
  TextureType type{};
  unsigned int id{};
  Texture(TextureType type = TextureType::Base, unsigned int id = 0)
    : type(type), id(id) {}
  std::string GetName() const override {
    return "Texture";
  }
  std::vector<Property> GetProperties() const override {
    return {{"Type", type}, {"ID", id}};
  }
  void SetProperty(Property property) override {
    if (std::holds_alternative<unsigned int>(property.value)) {
      auto& value = std::get<unsigned int>(property.value);
      if (property.name == "ID")
        id = value;
    } else if (std::holds_alternative<TextureType>(property.value)) {
      auto& value = std::get<TextureType>(property.value);
      if (property.name == "Type")
        type = value;
    }
  }
};
