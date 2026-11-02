#include <cmath>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/quaternion_transform.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <glm/fwd.hpp>
#include <glm/geometric.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>
#include <iostream>
#include <ostream>
#include <transform.hpp>
namespace kuki {
auto Transform::Update() -> void {
  const auto I = glm::mat4(1.f);
  const auto T = glm::translate(I, position);
  const auto R = glm::toMat4(rotation);
  const auto S = glm::scale(I, scale);
  local = T * R * S;
}
auto Transform::Reparent(const glm::mat4 *parent, glm::mat4 &world, bool keepWorld) -> void {
  if (!parent)
    local = world;
  else if (keepWorld)
    local = glm::inverse(*parent) * world;
  if (!parent || keepWorld) {
    glm::vec3 skew;
    glm::vec4 perspective;
    glm::decompose(local, scale, rotation, position, skew, perspective);
  }
  if (parent)
    world = *parent * local;
}
auto LookRotation(const glm::vec3 &direction) -> glm::quat {
  const auto length = glm::length(direction);
  if (length < 1e-6f)
    return glm::quat(1.f, .0f, .0f, .0f);
  const auto forward = direction / length;
  const auto up = std::abs(forward.y) > .999f ? glm::vec3(.0f, .0f, 1.f) : glm::vec3(.0f, 1.f, .0f);
  return glm::quatLookAt(forward, up);
}
auto WorldPosition(const glm::mat4 &world) -> glm::vec3 {
  return glm::vec3(world[3]);
}
auto WorldForward(const glm::mat4 &world) -> glm::vec3 {
  const auto axis = -glm::vec3(world[2]);
  const auto length = glm::length(axis);
  return length > 1e-6f ? axis / length : glm::vec3(.0f, .0f, -1.f);
}
auto WorldUp(const glm::mat4 &world) -> glm::vec3 {
  const auto axis = glm::vec3(world[1]);
  const auto length = glm::length(axis);
  return length > 1e-6f ? axis / length : glm::vec3(.0f, 1.f, .0f);
}
auto ViewFrom(const glm::mat4 &world) -> glm::mat4 {
  const auto position = WorldPosition(world);
  return glm::lookAt(position, position + WorldForward(world), WorldUp(world));
}
auto operator<<(std::ostream &os, const glm::mat4 &m) -> std::ostream & {
  for (auto row = 0; row < 4; ++row) {
    os << "| ";
    for (auto col = 0; col < 4; ++col)
      os << m[col][row] << " ";
    os << "|\n";
  }
  return os;
}
} // namespace kuki
