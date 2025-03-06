#include <component/component.hpp>
#include <component/mesh.hpp>
#include <glm/ext/vector_float3.hpp>
#include <string>
#include <variant>
#include <vector>
const std::string Mesh::GetName() const {
  return ComponentTraits<Mesh>::GetName();
}
std::vector<Property> Mesh::GetProperties() const {
  return {{"VertexArray", vertexArray}, {"VertexBuffer", vertexBuffer}, {"IndexBuffer", indexBuffer}, {"VertexCount", vertexCount}, {"IndexCount", indexCount}, {"MinBound", minBound}, {"MaxBound", maxBound}};
}
void Mesh::SetProperty(Property property) {
  if (std::holds_alternative<unsigned int>(property.value)) {
    auto& value = std::get<unsigned int>(property.value);
    if (property.name == "VertexArray")
      vertexArray = value;
    else if (property.name == "VertexBuffer")
      vertexBuffer = value;
    else if (property.name == "IndexBuffer")
      indexBuffer = value;
  } else if (std::holds_alternative<int>(property.value)) {
    auto& value = std::get<int>(property.value);
    if (property.name == "VertexCount")
      vertexCount = value;
    else if (property.name == "IndexCount")
      indexCount = value;
  } else if (std::holds_alternative<glm::vec3>(property.value)) {
    auto& value = std::get<glm::vec3>(property.value);
    if (property.name == "MinBound")
      minBound = value;
    else if (property.name == "MaxBound")
      maxBound = value;
  }
}
