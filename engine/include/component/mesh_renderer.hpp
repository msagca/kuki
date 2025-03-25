#pragma once
#include "component.hpp"
#include "component/material.hpp"
#include <engine_export.h>
#include <vector>
struct ENGINE_API MeshRenderer final : IComponent {
  int shader{};
  Material material{};
  const std::string GetName() const override;
  std::vector<Property> GetProperties() const override;
  void SetProperty(Property) override;
};
