#include <component/component.hpp>
#include <component/mesh.hpp>
#include <component/transform.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>
namespace kuki {
BoundingBox::BoundingBox()
  : min(glm::vec3(std::numeric_limits<float>::max())), max(glm::vec3(std::numeric_limits<float>::lowest())) {}
BoundingBox::BoundingBox(glm::vec3 min, glm::vec3 max) {
  if (glm::any(glm::lessThan(max, min)))
    throw std::invalid_argument("Max bounds must be greater than or equal to min bounds.");
  this->min = min;
  this->max = max;
}
BoundingBox BoundingBox::GetWorldBounds(const Transform* transform) {
  BoundingBox worldBounds;
  std::vector<glm::vec3> corners(8);
  corners[0] = glm::vec3(min.x, min.y, min.z);
  corners[1] = glm::vec3(max.x, min.y, min.z);
  corners[2] = glm::vec3(min.x, max.y, min.z);
  corners[3] = glm::vec3(max.x, max.y, min.z);
  corners[4] = glm::vec3(min.x, min.y, max.z);
  corners[5] = glm::vec3(max.x, min.y, max.z);
  corners[6] = glm::vec3(min.x, max.y, max.z);
  corners[7] = glm::vec3(max.x, max.y, max.z);
  worldBounds.min = glm::vec3(std::numeric_limits<float>::max());
  worldBounds.max = glm::vec3(std::numeric_limits<float>::lowest());
  for (const auto& corner : corners) {
    auto transformedCorner = transform->world * glm::vec4(corner, 1.0f);
    auto worldCorner = glm::vec3(transformedCorner) / transformedCorner.w;
    worldBounds.min = glm::min(worldBounds.min, worldCorner);
    worldBounds.max = glm::max(worldBounds.max, worldCorner);
  }
  return worldBounds;
}
const std::string Mesh::GetName() const {
  return ComponentTraits<Mesh>::GetName();
}
std::vector<Property> Mesh::GetProperties() const {
  return {{"VertexArray", vertexArray}, {"VertexBuffer", vertexBuffer}, {"IndexBuffer", indexBuffer}, {"VertexCount", vertexCount}, {"IndexCount", indexCount}, {"MinBound", bounds.min}, {"MaxBound", bounds.max}};
}
void Mesh::SetProperty(Property property) {
  if (auto value = std::get_if<int>(&property.value)) {
    if (property.name == "VertexArray")
      vertexArray = *value;
    else if (property.name == "VertexBuffer")
      vertexBuffer = *value;
    else if (property.name == "IndexBuffer")
      indexBuffer = *value;
    else if (property.name == "VertexCount")
      vertexCount = *value;
    else if (property.name == "IndexCount")
      indexCount = *value;
  } else if (auto value = std::get_if<glm::vec3>(&property.value)) {
    if (property.name == "MinBound")
      bounds.min = *value;
    else if (property.name == "MaxBound")
      bounds.max = *value;
  }
}
} // namespace kuki
