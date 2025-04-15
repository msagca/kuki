#pragma once
#include "component.hpp"
#include "texture.hpp"
#include <kuki_export.h>
#include <typeindex>
class Shader;
struct IMaterial : IComponent {
  virtual ~IMaterial() = default;
  /// @brief Apply this material to the given shader (set shader properties)
  virtual void Apply(Shader& shader) const = 0;
};
struct LitFallbackData {
  // NOTE: this is for use in offsetof() calls, LitFallback is not a standard-layout type
  glm::vec3 albedo{1.0f};
  float metalness{.5f};
  float occlusion{1.0f};
  float roughness{.5f};
};
/// @brief Fallback values to be used for lit materials in the absence of textures
struct KUKI_API LitFallback final : IMaterial {
  LitFallbackData data{};
  void Apply(Shader&) const override;
  const std::string GetName() const override;
  std::vector<Property> GetProperties() const override;
  void SetProperty(Property property) override;
};
/// @brief Fallback values to be used for unlit materials in the absence of textures
struct KUKI_API UnlitFallback final : IMaterial {
  glm::vec4 base{1.0f};
  void Apply(Shader&) const override;
  const std::string GetName() const override;
  std::vector<Property> GetProperties() const override;
  void SetProperty(Property property) override;
};
struct KUKI_API LitMaterial final : IMaterial {
  Texture albedo{};
  Texture normal{};
  Texture metalness{};
  Texture occlusion{};
  Texture roughness{};
  LitFallback fallback{};
  void Apply(Shader&) const override;
  const std::string GetName() const override;
  std::vector<Property> GetProperties() const override;
  void SetProperty(Property property) override;
};
struct KUKI_API UnlitMaterial final : IMaterial {
  Texture base{};
  UnlitFallback fallback{};
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
