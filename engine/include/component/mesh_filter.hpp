#pragma once
#include "component.hpp"
#include <engine_export.h>
#include "mesh.hpp"
#include <string>
#include <vector>
struct ENGINE_API MeshFilter : IComponent {
  Mesh mesh{};
  std::string GetName() const override;
  std::vector<Property> GetProperties() const override;
  void SetProperty(Property) override;
};
