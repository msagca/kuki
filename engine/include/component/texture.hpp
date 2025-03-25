#pragma once
#include "component.hpp"
#include <engine_export.h>
#include <vector>
struct ENGINE_API Texture final : IComponent {
  TextureType type{};
  int id{};
  const std::string GetName() const override;
  std::vector<Property> GetProperties() const override;
  void SetProperty(Property) override;
};
