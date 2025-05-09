#pragma once
#include "component.hpp"
#include "mesh.hpp"
#include <kuki_engine_export.h>
#include <vector>
namespace kuki {
struct KUKI_ENGINE_API MeshFilter final : IComponent {
  Mesh mesh{};
  const std::string GetName() const override;
  std::vector<Property> GetProperties() const override;
  void SetProperty(Property) override;
};
} // namespace kuki
