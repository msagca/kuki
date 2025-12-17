#include <component.hpp>
#include <glad/glad.h>
#include <material.hpp>
#include <shader.hpp>
#include <type_traits>
#include <typeindex>
#include <utility>
#include <variant>
namespace kuki {
Material::Material()
  : IComponent(std::in_place_type<Material>) {}
Material::Material(const Material& other)
  : IComponent(other), current(other.current) {}
void Material::Apply(Shader* shader) const {
  std::visit([shader](const auto& material) { material.Apply(shader); }, current);
}
Material& Material::operator=(const Material& other) {
  return std::visit(
    [&](auto& destination, const auto& source) -> Material& {
      using D = std::decay_t<decltype(destination)>;
      using S = std::decay_t<decltype(source)>;
      if constexpr (std::is_same_v<D, S>)
        destination = source;
      return *this;
    },
    current,
    other.current);
}
std::type_index Material::GetTypeIndex() const {
  return std::visit([](const auto& material) -> std::type_index { return typeid(material); }, current);
}
MaterialType Material::GetType() const {
  return std::visit([](const auto& material) -> MaterialType { return material.type; }, current);
}
void Material::SetType(MaterialType type) {
  if (type == GetType())
    return;
  if (auto litMaterial = std::get_if<LitMaterial>(&current)) {
    auto color = litMaterial->fallback.albedo;
    current = UnlitMaterial{};
    auto unlitMaterial = std::get_if<UnlitMaterial>(&current);
    unlitMaterial->fallback.base = color;
  } else if (auto unlitMaterial = std::get_if<UnlitMaterial>(&current)) {
    auto color = unlitMaterial->fallback.base;
    current = LitMaterial{};
    auto litMaterial = std::get_if<LitMaterial>(&current);
    litMaterial->fallback.albedo = color;
  }
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
void UnlitMaterial::Apply(Shader* shader) const {
  if (type != shader->GetType())
    return;
  if (data.base > 0) {
    glActiveTexture(GL_TEXTURE0);
    if (type == MaterialType::Skybox) {
      glBindTexture(GL_TEXTURE_CUBE_MAP, data.base);
      shader->SetUniform("skybox", 0);
    } else if (type == MaterialType::CubeMapEquirect) {
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
} // namespace kuki
