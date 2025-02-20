#pragma once
#include "component.hpp"
#include <engine_export.h>
#include <string>
#include <vector>
struct ENGINE_API Texture : IComponent {
  TextureType type{};
  unsigned int id{};
  std::string GetName() const override;
  std::vector<Property> GetProperties() const override;
  void SetProperty(Property) override;
};
