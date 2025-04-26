#include <component/component.hpp>
#include <component/material.hpp>
#include <component/shader.hpp>
#include <glad/glad.h>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <string>
#include <typeindex>
#include <variant>
#include <vector>
namespace kuki {
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
void LitMaterial::Apply(Shader& shader) const {
  if (data.albedo > 0) {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, data.albedo);
    shader.SetUniform("material.albedo", 0);
  }
  if (data.normal > 0) {
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, data.normal);
    shader.SetUniform("material.normal", 1);
  }
  if (data.metalness > 0) {
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, data.metalness);
    shader.SetUniform("material.metalness", 2);
  }
  if (data.occlusion > 0) {
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, data.occlusion);
    shader.SetUniform("material.occlusion", 3);
  }
  if (data.roughness > 0) {
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, data.roughness);
    shader.SetUniform("material.roughness", 4);
  }
}
const std::string LitMaterial::GetName() const {
  return "LitMaterial";
}
std::vector<Property> LitMaterial::GetProperties() const {
  return {{"AlbedoTexture", data.albedo}, {"NormalTexture", data.normal}, {"MetalnessTexture", data.metalness}, {"OcclusionTexture", data.occlusion}, {"RoughnessTexture", data.roughness}, {"AlbedoColor", fallback.albedo, PropertyType::Color}, {"MetalnessFactor", fallback.metalness, PropertyType::NumberRange}, {"OcclusionFactor", fallback.occlusion, PropertyType::NumberRange}, {"RoughnessFactor", fallback.roughness, PropertyType::NumberRange}, {"TextureMask", fallback.textureMask}};
}
void LitMaterial::SetProperty(Property property) {
  if (std::holds_alternative<int>(property.value)) {
    auto& value = std::get<int>(property.value);
    if (property.name == "AlbedoTexture")
      data.albedo = value;
    else if (property.name == "NormalTexture")
      data.normal = value;
    else if (property.name == "MetalnessTexture")
      data.metalness = value;
    else if (property.name == "OcclusionTexture")
      data.occlusion = value;
    else if (property.name == "RoughnessTexture")
      data.roughness = value;
    else if (property.name == "TextureMask")
      fallback.textureMask = value;
  } else if (std::holds_alternative<glm::vec3>(property.value)) {
    auto& value = std::get<glm::vec3>(property.value);
    if (property.name == "AlbedoColor")
      fallback.albedo = value;
  } else if (std::holds_alternative<float>(property.value)) {
    auto& value = std::get<float>(property.value);
    if (property.name == "MetalnessFactor")
      fallback.metalness = value;
    else if (property.name == "OcclusionFactor")
      fallback.occlusion = value;
    else if (property.name == "RoughnessFactor")
      fallback.roughness = value;
  }
}
void UnlitMaterial::Apply(Shader& shader) const {
  if (data.base > 0) {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, data.base);
    shader.SetUniform("material.base", 0);
  }
}
const std::string UnlitMaterial::GetName() const {
  return "UnlitMaterial";
}
std::vector<Property> UnlitMaterial::GetProperties() const {
  return {{"BaseTexture", data.base}, {"BaseColor", fallback.base, PropertyType::Color}, {"TextureMask", fallback.textureMask}};
}
void UnlitMaterial::SetProperty(Property property) {
  if (std::holds_alternative<glm::vec4>(property.value)) {
    auto& value = std::get<glm::vec4>(property.value);
    fallback.base = value;
  } else if (std::holds_alternative<int>(property.value)) {
    auto& value = std::get<int>(property.value);
    if (property.name == "BaseTexture")
      data.base = value;
    else if (property.name == "TextureMask")
      fallback.textureMask = value;
  }
}
} // namespace kuki
