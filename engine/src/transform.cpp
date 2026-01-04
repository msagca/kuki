#define GLM_ENABLE_EXPERIMENTAL
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <glm/fwd.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>
#include <iostream>
#include <ostream>
#include <transform.hpp>
#include <utility>
namespace kuki {
auto Transform::operator=(const Transform &other) -> Transform & {
  // NOTE: this override does not copy the parent ID
  position = other.position;
  rotation = other.rotation;
  scale = other.scale;
  local = other.local;
  world = other.world;
  dirty = true;
  return *this;
}
auto Transform::Update(const Transform *parent) -> void {
  const auto I = glm::mat4(1.f);
  const auto T = glm::translate(I, position);
  const auto R = glm::toMat4(rotation);
  const auto S = glm::scale(I, scale);
  local = T * R * S;
  if (parent)
    world = parent->world * local;
  else
    world = local;
}
auto Transform::Reparent(const Transform *parent, bool keepWorld) -> void {
  if (!parent)
    local = world;
  else if (keepWorld)
    local = glm::inverse(parent->world) * world;
  if (!parent || keepWorld) {
    glm::vec3 skew;
    glm::vec4 perspective;
    glm::decompose(local, scale, rotation, position, skew, perspective);
  }
  if (parent)
    world = parent->world * local;
}
auto operator<<(std::ostream &os, const glm::mat4 &m) -> std::ostream & {
  for (auto row = 0; row < 4; ++row) {
    os << "| ";
    for (auto col = 0; col < 4; ++col)
      os << m[col][row] << " ";
    os << "|" << std::endl;
  }
  return os;
}
} // namespace kuki
