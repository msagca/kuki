#include <component/component.hpp>
#include <component/mesh_renderer.hpp>
#include <string>
#include <variant>
#include <vector>
std::string MeshRenderer::GetName() const {
  return "MeshRenderer";
}
std::vector<Property> MeshRenderer::GetProperties() const {
  std::vector<Property> properties = {{"Shader", shader}};
  auto matProperties = material.GetProperties();
  properties.insert(properties.end(), matProperties.begin(), matProperties.end());
  return properties;
}
void MeshRenderer::SetProperty(Property property) {
  if (std::holds_alternative<unsigned int>(property.value)) {
    auto& value = std::get<unsigned int>(property.value);
    if (property.name == "Shader")
      shader = value;
  }
  material.SetProperty(property);
}
