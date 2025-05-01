#include <component/component.hpp>
#include <component/mesh_renderer.hpp>
#include <string>
#include <variant>
#include <vector>
namespace kuki {
const std::string MeshRenderer::GetName() const {
  return ComponentTraits<MeshRenderer>::GetName();
}
std::vector<Property> MeshRenderer::GetProperties() const {
  return material.GetProperties();
}
void MeshRenderer::SetProperty(Property property) {
  material.SetProperty(property);
}
} // namespace kuki
