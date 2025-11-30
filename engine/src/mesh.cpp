#include <component.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <limits>
#include <mesh.hpp>
#include <stdexcept>
#include <utility>
namespace kuki {
Mesh::Mesh()
  : IComponent(std::in_place_type<Mesh>) {}
BoundingBox::BoundingBox()
  : min(glm::vec3(std::numeric_limits<float>::max())), max(glm::vec3(std::numeric_limits<float>::lowest())) {}
BoundingBox::BoundingBox(glm::vec3 min, glm::vec3 max) {
  if (glm::any(glm::lessThan(max, min)))
    throw std::invalid_argument("Max bounds must be greater than or equal to min bounds.");
  this->min = min;
  this->max = max;
}
BoundingBox BoundingBox::GetWorldBounds(const glm::mat4& transform) {
  glm::vec3 corners[8] = {{min.x, min.y, min.z}, {max.x, min.y, min.z}, {min.x, max.y, min.z}, {max.x, max.y, min.z}, {min.x, min.y, max.z}, {max.x, min.y, max.z}, {min.x, max.y, max.z}, {max.x, max.y, max.z}};
  BoundingBox bounds{};
  for (const auto& v : corners) {
    auto v4 = transform * glm::vec4(v, 1.0f);
    auto v3 = glm::vec3(v4);
    bounds.min = glm::min(bounds.min, v3);
    bounds.max = glm::max(bounds.max, v3);
  }
  return bounds;
}
} // namespace kuki
