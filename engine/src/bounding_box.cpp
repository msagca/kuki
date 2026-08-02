#include <bounding_box.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/scalar_constants.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <primitive.hpp>
#include <span>
namespace kuki {
BoundingBox BoundingBox::GetWorldBounds(const glm::mat4 &transform) const {
  BoundingBox bounds{};
  const glm::vec3 corners[8] = {{min.x, min.y, min.z}, {max.x, min.y, min.z}, {min.x, max.y, min.z}, {max.x, max.y, min.z}, {min.x, min.y, max.z}, {max.x, min.y, max.z}, {min.x, max.y, max.z}, {max.x, max.y, max.z}};
  for (const auto &v : corners) {
    auto v4 = transform * glm::vec4(v, 1.f);
    auto v3 = glm::vec3(v4);
    bounds.min = glm::min(bounds.min, v3);
    bounds.max = glm::max(bounds.max, v3);
  }
  return bounds;
}
BoundingBox::operator bool() const {
  return glm::all(glm::greaterThan(max - min, glm::vec3(glm::epsilon<float>())));
}
auto BoundingBox::Calculate(std::span<const glm::vec3> vertices) -> BoundingBox {
  BoundingBox bounds{};
  for (const auto &v : vertices) {
    bounds.min = glm::min(bounds.min, v);
    bounds.max = glm::max(bounds.max, v);
  }
  return bounds;
}
auto BoundingBox::Calculate(std::span<const Vertex> vertices) -> BoundingBox {
  BoundingBox bounds{};
  for (const auto &v : vertices) {
    bounds.min = glm::min(bounds.min, v.position);
    bounds.max = glm::max(bounds.max, v.position);
  }
  return bounds;
}
} // namespace kuki
