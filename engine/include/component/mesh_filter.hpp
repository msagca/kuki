#pragma once
#include "component.hpp"
#include "mesh.hpp"
#include <engine_export.h>
#include <vector>
struct ENGINE_API MeshFilter : IComponent {
  Mesh mesh{};
  const std::string GetName() const override;
  std::vector<Property> GetProperties() const override;
  void SetProperty(Property) override;
};
