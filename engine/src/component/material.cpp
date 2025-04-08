#include <component/component.hpp>
#include <component/material.hpp>
#include <component/shader.hpp>
#include <glad/glad.h>
#include <glm/ext/vector_float3.hpp>
#include <string>
#include <variant>
#include <vector>
void Material::Apply(Shader& shader) const {
  std::visit([&shader](const auto& mat) { mat.Apply(shader); }, material);
}
std::type_index Material::GetTypeIndex() const {
  return std::visit([](const auto& mat) -> std::type_index { return typeid(mat); }, material);
}
const std::string Material::GetName() const {
  return ComponentTraits<Material>::GetName();
}
std::vector<Property> Material::GetProperties() const {
  return std::visit([](const auto& mat) { return mat.GetProperties(); }, material);
}
void Material::SetProperty(Property property) {
  std::visit([&property](auto& mat) { mat.SetProperty(property); }, material);
}
void LitFallback::Apply(Shader& shader) const {
  shader.SetUniform("fallback.albedo", albedo);
  shader.SetUniform("fallback.metalness", metalness);
  shader.SetUniform("fallback.occlusion", occlusion);
  shader.SetUniform("fallback.roughness", roughness);
}
const std::string LitFallback::GetName() const {
  return "LitFallback";
}
std::vector<Property> LitFallback::GetProperties() const {
  return {{"AlbedoColor", albedo, PropertyType::Color}, {"MetalnessFactor", metalness}, {"OcclusionFactor", occlusion}, {"RoughnessFactor", roughness}};
}
void LitFallback::SetProperty(Property property) {
  if (std::holds_alternative<glm::vec3>(property.value)) {
    auto& value = std::get<glm::vec3>(property.value);
    if (property.name == "AlbedoColor")
      albedo = value;
  } else if (std::holds_alternative<float>(property.value)) {
    auto& value = std::get<float>(property.value);
    if (property.name == "MetalnessFactor")
      metalness = value;
    else if (property.name == "OcclusionFactor")
      occlusion = value;
    else if (property.name == "RoughnessFactor")
      roughness = value;
  }
}
void UnlitFallback::Apply(Shader& shader) const {
  shader.SetUniform("fallback.base", base);
}
const std::string UnlitFallback::GetName() const {
  return "UnlitFallback";
}
std::vector<Property> UnlitFallback::GetProperties() const {
  return {{"BaseColor", base, PropertyType::Color}};
}
void UnlitFallback::SetProperty(Property property) {
  if (std::holds_alternative<glm::vec4>(property.value)) {
    auto& value = std::get<glm::vec4>(property.value);
    base = value;
  }
}
void LitMaterial::Apply(Shader& shader) const {
  if (glIsTexture(albedo.id)) {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, albedo.id);
    shader.SetUniform("material.albedo", 0);
    shader.SetUniform("useAlbedoFallback", false);
  } else
    shader.SetUniform("useAlbedoFallback", true);
  if (glIsTexture(normal.id)) {
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, normal.id);
    shader.SetUniform("material.normal", 1);
    shader.SetUniform("useNormalFallback", false);
  } else
    shader.SetUniform("useNormalFallback", true);
  if (glIsTexture(metalness.id)) {
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, metalness.id);
    shader.SetUniform("material.metalness", 2);
    shader.SetUniform("useMetalnessFallback", false);
  } else
    shader.SetUniform("useMetalnessFallback", true);
  if (glIsTexture(occlusion.id)) {
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, occlusion.id);
    shader.SetUniform("material.occlusion", 3);
    shader.SetUniform("useOcclusionFallback", false);
  } else
    shader.SetUniform("useOcclusionFallback", true);
  if (glIsTexture(roughness.id)) {
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, roughness.id);
    shader.SetUniform("material.roughness", 4);
    shader.SetUniform("useRoughnessFallback", false);
  } else
    shader.SetUniform("useRoughnessFallback", true);
  fallback.Apply(shader);
}
const std::string LitMaterial::GetName() const {
  return "LitMaterial";
}
std::vector<Property> LitMaterial::GetProperties() const {
  auto properties = albedo.GetProperties();
  auto normalProperties = normal.GetProperties();
  auto metalnessProperties = metalness.GetProperties();
  auto occlusionProperties = occlusion.GetProperties();
  auto roughnessProperties = roughness.GetProperties();
  auto fallbackProperties = fallback.GetProperties();
  properties.insert(properties.end(), normalProperties.begin(), normalProperties.end());
  properties.insert(properties.end(), metalnessProperties.begin(), metalnessProperties.end());
  properties.insert(properties.end(), occlusionProperties.begin(), occlusionProperties.end());
  properties.insert(properties.end(), roughnessProperties.begin(), roughnessProperties.end());
  properties.insert(properties.end(), fallbackProperties.begin(), fallbackProperties.end());
  return properties;
}
void LitMaterial::SetProperty(Property property) {
  if (property.name == "Albedo")
    albedo.SetProperty(property);
  else if (property.name == "Normal")
    normal.SetProperty(property);
  else if (property.name == "Metalness")
    metalness.SetProperty(property);
  else if (property.name == "Occlusion")
    occlusion.SetProperty(property);
  else if (property.name == "Roughness")
    roughness.SetProperty(property);
  else
    fallback.SetProperty(property);
}
void UnlitMaterial::Apply(Shader& shader) const {
  if (glIsTexture(base.id)) {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, base.id);
    shader.SetUniform("material.base", 0);
    shader.SetUniform("useBaseFallback", false);
  } else
    shader.SetUniform("useBaseFallback", true);
  shader.SetUniform("fallback.base", fallback.base);
}
const std::string UnlitMaterial::GetName() const {
  return "UnlitMaterial";
}
std::vector<Property> UnlitMaterial::GetProperties() const {
  auto properties = base.GetProperties();
  auto fallbackProperties = fallback.GetProperties();
  properties.insert(properties.end(), fallbackProperties.begin(), fallbackProperties.end());
  return properties;
}
void UnlitMaterial::SetProperty(Property property) {
  if (property.name == "Base")
    base.SetProperty(property);
  else
    fallback.SetProperty(property);
}
