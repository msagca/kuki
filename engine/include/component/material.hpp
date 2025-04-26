#pragma once
#include "component.hpp"
#include "texture.hpp"
#include <kuki_export.h>
#include <typeindex>
namespace kuki {
class Shader;
struct LitData {
  int albedo{};
  int normal{};
  int metalness{};
  int occlusion{};
  int roughness{};
};
struct LitFallbackData {
  glm::vec3 albedo{1.0f};
  float metalness{.5f};
  float occlusion{1.0f};
  float roughness{.5f};
  int textureMask{0};
};
struct UnlitData {
  int base{};
};
struct UnlitFallbackData {
  glm::vec4 base{1.0f};
  int textureMask{0};
};
struct IMaterial : IComponent {
  virtual ~IMaterial() = default;
  /// @brief Apply this material to the given shader (set shader properties)
  virtual void Apply(Shader& shader) const = 0;
};
struct KUKI_API LitMaterial final : IMaterial {
  LitData data{};
  LitFallbackData fallback{};
  void Apply(Shader&) const override;
  const std::string GetName() const override;
  std::vector<Property> GetProperties() const override;
  void SetProperty(Property property) override;
};
struct KUKI_API UnlitMaterial final : IMaterial {
  UnlitData data{};
  UnlitFallbackData fallback{};
  void Apply(Shader&) const override;
  const std::string GetName() const override;
  std::vector<Property> GetProperties() const override;
  void SetProperty(Property property) override;
};
struct KUKI_API Material : IMaterial {
  std::variant<LitMaterial, UnlitMaterial> material;
  void Apply(Shader& shader) const override;
  std::type_index GetTypeIndex() const;
  const std::string GetName() const override;
  std::vector<Property> GetProperties() const override;
  void SetProperty(Property) override;
};
} // namespace kuki
