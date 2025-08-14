#pragma once
#include "component.hpp"
#include <kuki_engine_export.h>
#include <typeindex>
#include <variant>
namespace kuki {
class Shader;
struct LitData {
  int albedo{};
  int normal{};
  int metalness{};
  int occlusion{};
  int roughness{};
  int specular{};
  int emissive{};
};
struct LitFallbackData {
  glm::vec4 albedo{1.0f};
  glm::vec4 specular{.0f};
  glm::vec4 emissive{.0f};
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
struct KUKI_ENGINE_API LitMaterial {
  MaterialType type{MaterialType::Lit};
  LitData data{};
  LitFallbackData fallback{};
  void Apply(Shader*) const;
  void CopyTo(LitMaterial&) const;
};
struct KUKI_ENGINE_API UnlitMaterial {
  MaterialType type{MaterialType::Unlit};
  UnlitData data{};
  UnlitFallbackData fallback{};
  void Apply(Shader*) const;
  void CopyTo(UnlitMaterial&) const;
};
struct KUKI_ENGINE_API Material final : public IComponent {
  Material()
    : IComponent(std::in_place_type<Material>) {}
  std::variant<LitMaterial, UnlitMaterial> material;
  std::type_index GetTypeIndex() const;
  MaterialType GetType() const;
  void Apply(Shader*) const;
  void CopyTo(Material&) const;
};
enum class MaterialProperty {
  AlbedoTexture,
  NormalTexture,
  MetalnessTexture,
  OcclusionTexture,
  RoughnessTexture,
  SpecularTexture,
  EmissiveTexture
};
} // namespace kuki
