#include <component/component.hpp>
#include <component/mesh_filter.hpp>
#include <string>
#include <vector>
std::string MeshFilter::GetName() const {
  return "MeshFilter";
}
std::vector<Property> MeshFilter::GetProperties() const {
  return mesh.GetProperties();
}
void MeshFilter::SetProperty(Property property) {
  mesh.SetProperty(property);
}
