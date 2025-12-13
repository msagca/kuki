#include <bounding_box.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <glm/geometric.hpp>
#include <plane.hpp>
namespace kuki {
Plane::Plane()
  : point(.0f), normal({.0f, 1.0f, .0f}) {}
Plane::Plane(const glm::vec3& point, const glm::vec3& normal)
  : point(point), normal(glm::normalize(normal)) {}
Plane::Plane(const glm::vec4& plane) {
  auto len = glm::length(glm::vec3(plane));
  auto p = plane / len;
  normal = glm::vec3(p);
  point = -p.w * normal;
}
bool Plane::OnPositiveSide(const BoundingBox& bounds) const {
  glm::vec3 positiveCorner{};
  positiveCorner.x = (normal.x >= 0) ? bounds.max.x : bounds.min.x;
  positiveCorner.y = (normal.y >= 0) ? bounds.max.y : bounds.min.y;
  positiveCorner.z = (normal.z >= 0) ? bounds.max.z : bounds.min.z;
  return SignedDistance(positiveCorner) >= 0;
}
float Plane::SignedDistance(const glm::vec3& point) const {
  return glm::dot(normal, point - this->point);
}
} // namespace kuki
