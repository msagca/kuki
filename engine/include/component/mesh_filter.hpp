#pragma once
#include "component.hpp"
#include "mesh.hpp"
#include <kuki_export.h>
#include <vector>
struct KUKI_API MeshFilter final : IComponent {
  Mesh mesh{};
  const std::string GetName() const override;
  std::vector<Property> GetProperties() const override;
  void SetProperty(Property) override;
};
