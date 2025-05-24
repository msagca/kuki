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
void Material::Apply(Shader* shader) const {
  std::visit([&shader](const auto& material) { material.Apply(shader); }, material);
}
std::type_index Material::GetTypeIndex() const {
  return std::visit([](const auto& material) -> std::type_index { return typeid(material); }, material);
}
MaterialType Material::GetType() const {
  return std::visit([](const auto& material) -> MaterialType { return material.type; }, material);
}
const std::string Material::GetName() const {
  return ComponentTraits<Material>::GetName();
}
std::vector<Property> Material::GetProperties() const {
  return std::visit([](const auto& material) { return material.GetProperties(); }, material);
}
void Material::SetProperty(Property property) {
  std::visit([&property](auto& material) { material.SetProperty(property); }, material);
}
void LitMaterial::Apply(Shader* shader) const {
  if (type != shader->GetType())
    return;
  if (data.albedo > 0) {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, data.albedo);
    shader->SetUniform("material.albedo", 0);
  }
  if (data.normal > 0) {
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, data.normal);
    shader->SetUniform("material.normal", 1);
  }
  if (data.metalness > 0) {
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, data.metalness);
    shader->SetUniform("material.metalness", 2);
  }
  if (data.occlusion > 0) {
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, data.occlusion);
    shader->SetUniform("material.occlusion", 3);
  }
  if (data.roughness > 0) {
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, data.roughness);
    shader->SetUniform("material.roughness", 4);
  }
  if (data.specular > 0) {
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, data.specular);
    shader->SetUniform("material.specular", 5);
  }
  if (data.emissive > 0) {
    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_2D, data.emissive);
    shader->SetUniform("material.emissive", 6);
  }
}
const std::string LitMaterial::GetName() const {
  return "LitMaterial";
}
std::vector<Property> LitMaterial::GetProperties() const {
  return {{"Type", type}, {"AlbedoTexture", data.albedo, PropertyType::Texture}, {"NormalTexture", data.normal, PropertyType::Texture}, {"MetalnessTexture", data.metalness, PropertyType::Texture}, {"OcclusionTexture", data.occlusion, PropertyType::Texture}, {"RoughnessTexture", data.roughness, PropertyType::Texture}, {"SpecularTexture", data.specular, PropertyType::Texture}, {"EmissiveTexture", data.emissive, PropertyType::Texture}, {"AlbedoColor", fallback.albedo, PropertyType::Color}, {"SpecularColor", fallback.specular, PropertyType::Color}, {"EmissiveColor", fallback.emissive, PropertyType::Color}, {"Metalness", fallback.metalness, PropertyType::NumberRange}, {"Occlusion", fallback.occlusion, PropertyType::NumberRange}, {"Roughness", fallback.roughness, PropertyType::NumberRange}, {"TextureMask", fallback.textureMask}};
}
void LitMaterial::SetProperty(Property property) {
  if (auto value = std::get_if<int>(&property.value)) {
    if (property.name == "AlbedoTexture")
      data.albedo = *value;
    else if (property.name == "NormalTexture")
      data.normal = *value;
    else if (property.name == "MetalnessTexture")
      data.metalness = *value;
    else if (property.name == "OcclusionTexture")
      data.occlusion = *value;
    else if (property.name == "RoughnessTexture")
      data.roughness = *value;
    else if (property.name == "SpecularTexture")
      data.specular = *value;
    else if (property.name == "EmissiveTexture")
      data.emissive = *value;
    else if (property.name == "TextureMask")
      fallback.textureMask = *value;
  } else if (auto value = std::get_if<glm::vec4>(&property.value)) {
    if (property.name == "AlbedoColor")
      fallback.albedo = *value;
    else if (property.name == "SpecularColor")
      fallback.specular = *value;
    else if (property.name == "EmissiveColor")
      fallback.emissive = *value;
  } else if (auto value = std::get_if<float>(&property.value)) {
    if (property.name == "Metalness")
      fallback.metalness = *value;
    else if (property.name == "Occlusion")
      fallback.occlusion = *value;
    else if (property.name == "Roughness")
      fallback.roughness = *value;
  } else if (auto value = std::get_if<MaterialType>(&property.value)) {
    if (property.name == "Type")
      type = *value;
  }
}
void UnlitMaterial::Apply(Shader* shader) const {
  if (type != shader->GetType())
    return;
  if (data.base > 0) {
    glActiveTexture(GL_TEXTURE0);
    if (type == MaterialType::Skybox) {
      glBindTexture(GL_TEXTURE_CUBE_MAP, data.base);
      shader->SetUniform("skybox", 0);
    } else if (type == MaterialType::CubeMapEquirect || type == MaterialType::CubeMapIrradiance) {
      glBindTexture(GL_TEXTURE_CUBE_MAP, data.base);
      shader->SetUniform("cubeMap", 0);
    } else if (type == MaterialType::EquirectCubeMap) {
      glBindTexture(GL_TEXTURE_2D, data.base);
      shader->SetUniform("equirect", 0);
    } else {
      glBindTexture(GL_TEXTURE_2D, data.base);
      shader->SetUniform("material.base", 0);
    }
  }
}
const std::string UnlitMaterial::GetName() const {
  return "UnlitMaterial";
}
std::vector<Property> UnlitMaterial::GetProperties() const {
  return {{"Type", type}, {"BaseTexture", data.base, PropertyType::Texture}, {"BaseColor", fallback.base, PropertyType::Color}, {"TextureMask", fallback.textureMask}};
}
void UnlitMaterial::SetProperty(Property property) {
  if (auto value = std::get_if<glm::vec4>(&property.value)) {
    if (property.name == "BaseColor")
      fallback.base = *value;
  } else if (auto value = std::get_if<int>(&property.value)) {
    if (property.name == "BaseTexture")
      data.base = *value;
    else if (property.name == "TextureMask")
      fallback.textureMask = *value;
  } else if (auto value = std::get_if<MaterialType>(&property.value)) {
    if (property.name == "Type")
      type = *value;
  }
}
} // namespace kuki
