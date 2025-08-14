#include <component/component.hpp>
#include <component/mesh.hpp>
#include <component/transform.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <limits>
#include <stdexcept>
#include <vector>
namespace kuki {
void Mesh::CopyTo(Mesh& other) const {
  other.vertexArray = vertexArray;
  other.vertexBuffer = vertexBuffer;
  other.indexBuffer = indexBuffer;
  other.vertexCount = vertexCount;
  other.indexCount = indexCount;
  other.bounds = bounds;
}
BoundingBox::BoundingBox()
  : min(glm::vec3(std::numeric_limits<float>::max())), max(glm::vec3(std::numeric_limits<float>::lowest())) {}
BoundingBox::BoundingBox(glm::vec3 min, glm::vec3 max) {
  if (glm::any(glm::lessThan(max, min)))
    throw std::invalid_argument("Max bounds must be greater than or equal to min bounds.");
  this->min = min;
  this->max = max;
}
BoundingBox BoundingBox::GetWorldBounds(glm::mat4 transform) {
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
    auto cornerWorld = transform * glm::vec4(corner, 1.0f);
    auto worldCorner = glm::vec3(cornerWorld) / cornerWorld.w;
    worldBounds.min = glm::min(worldBounds.min, worldCorner);
    worldBounds.max = glm::max(worldBounds.max, worldCorner);
  }
  return worldBounds;
}
} // namespace kuki
