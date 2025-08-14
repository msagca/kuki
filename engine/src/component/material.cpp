#include <glad/glad.h>
#include <component/component.hpp>
#include <component/material.hpp>
#include <component/shader.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <string>
#include <typeindex>
#include <variant>
namespace kuki {
void Material::Apply(Shader* shader) const {
  std::visit([shader](const auto& material) { material.Apply(shader); }, material);
}
struct CopyMatching {
  void operator()(const LitMaterial& source, LitMaterial& destination) const {
    source.CopyTo(destination);
  }
  void operator()(const UnlitMaterial& source, UnlitMaterial& destination) const {
    source.CopyTo(destination);
  }
  template <typename S, typename D>
  void operator()(const S&, D&) const {}
};
void Material::CopyTo(Material& other) const {
  std::visit(CopyMatching{}, material, other.material);
}
std::type_index Material::GetTypeIndex() const {
  return std::visit([](const auto& material) -> std::type_index { return typeid(material); }, material);
}
MaterialType Material::GetType() const {
  return std::visit([](const auto& material) -> MaterialType { return material.type; }, material);
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
void LitMaterial::CopyTo(LitMaterial& other) const {
  other.data = data;
  other.fallback = fallback;
  other.type = type;
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
void UnlitMaterial::CopyTo(UnlitMaterial& other) const {
  other.data = data;
  other.fallback = fallback;
  other.type = type;
}
} // namespace kuki
