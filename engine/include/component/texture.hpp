#pragma once
#include "component.hpp"
#include <kuki_export.h>
#include <vector>
namespace kuki {
struct KUKI_API Texture final : IComponent {
  TextureType type{};
  int width{};
  int height{};
  int id{};
  const std::string GetName() const override;
  std::vector<Property> GetProperties() const override;
  void SetProperty(Property) override;
};
} // namespace kuki
