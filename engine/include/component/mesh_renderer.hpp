#pragma once
#include "component.hpp"
#include "component/material.hpp"
#include <kuki_export.h>
#include <vector>
namespace kuki {
struct KUKI_API MeshRenderer final : IComponent {
  int shader{};
  Material material{};
  const std::string GetName() const override;
  std::vector<Property> GetProperties() const override;
  void SetProperty(Property) override;
};
} // namespace kuki
