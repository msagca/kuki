#include <component/component.hpp>
#include <component/mesh_filter.hpp>
#include <string>
#include <vector>
namespace kuki {
const std::string MeshFilter::GetName() const {
  return ComponentTraits<MeshFilter>::GetName();
}
std::vector<Property> MeshFilter::GetProperties() const {
  return mesh.GetProperties();
}
void MeshFilter::SetProperty(Property property) {
  mesh.SetProperty(property);
}
} // namespace kuki
