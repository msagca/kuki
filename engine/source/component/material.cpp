#include <component/component.hpp>
#include <component/material.hpp>
#include <component/shader.hpp>
#include <string>
#include <variant>
#include <vector>
void PhongMaterial::Apply(Shader& shader) const {
  shader.SetUniform("material.diffuse", diffuse);
  shader.SetUniform("material.specular", specular);
  shader.SetUniform("material.shininess", shininess);
}
std::string PhongMaterial::GetName() const {
  return "PhongMaterial";
}
std::vector<Property> PhongMaterial::GetProperties() const {
  return {{"Diffuse", diffuse}, {"Specular", specular}, {"Shininess", shininess}};
}
void PhongMaterial::SetProperty(Property property) {
  if (std::holds_alternative<glm::vec3>(property.value)) {
    auto& value = std::get<glm::vec3>(property.value);
    if (property.name == "Diffuse")
      diffuse = value;
    else if (property.name == "Specular")
      specular = value;
  } else if (std::holds_alternative<float>(property.value)) {
    auto& value = std::get<float>(property.value);
    if (property.name == "Shininess")
      shininess = value;
  }
}
void PBRMaterial::Apply(Shader& shader) const {
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, base);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, normal);
  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, metalness);
  glActiveTexture(GL_TEXTURE3);
  glBindTexture(GL_TEXTURE_2D, occlusion);
  glActiveTexture(GL_TEXTURE4);
  glBindTexture(GL_TEXTURE_2D, roughness);
  shader.SetUniform("material.base", 0);
  shader.SetUniform("material.normal", 1);
  shader.SetUniform("material.metalness", 2);
  shader.SetUniform("material.occlusion", 3);
  shader.SetUniform("material.roughness", 4);
}
std::string PBRMaterial::GetName() const {
  return "PBRMaterial";
}
std::vector<Property> PBRMaterial::GetProperties() const {
  return {{"Base", base}, {"Normal", normal}, {"Metalness", metalness}, {"Occlusion", occlusion}, {"Roughness", roughness}};
}
void PBRMaterial::SetProperty(Property property) {
  if (std::holds_alternative<unsigned int>(property.value)) {
    auto& value = std::get<unsigned int>(property.value);
    if (property.name == "Base")
      base = value;
    else if (property.name == "Normal")
      normal = value;
    else if (property.name == "Metalness")
      metalness = value;
    else if (property.name == "Occlusion")
      occlusion = value;
    else if (property.name == "Roughness")
      roughness = value;
  }
}
void Material::Apply(Shader& shader) const {
  std::visit([&shader](const auto& mat) { mat.Apply(shader); }, material);
}
std::string Material::GetName() const {
  return std::visit([](const auto& mat) { return mat.GetName(); }, material);
}
std::vector<Property> Material::GetProperties() const {
  return std::visit([](const auto& mat) { return mat.GetProperties(); }, material);
}
void Material::SetProperty(Property property) {
  std::visit([&property](auto& mat) { mat.SetProperty(property); }, material);
}
