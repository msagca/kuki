#define GLM_ENABLE_EXPERIMENTAL
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/quaternion_float.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/gtx/quaternion.hpp>
#include <light.hpp>
#include <transform.hpp>
namespace kuki {
auto Light::GetModel() const -> glm::mat4 {
  return glm::translate(glm::mat4(1.f), position) * glm::toMat4(rotation);
}
auto Light::GetTransform() const -> Transform {
  Transform transform;
  transform.position = position;
  transform.rotation = rotation;
  transform.local = GetModel();
  transform.world = transform.local; // TODO: do not assume world = local
  return transform;
}
auto Light::GetView() const -> glm::mat4 {
  static constexpr auto WORLD_FORWARD = glm::vec3(.0f, .0f, -1.f);
  static constexpr auto WORLD_UP = glm::vec3(.0f, 1.f, .0f);
  auto forward = rotation * WORLD_FORWARD;
  auto up = rotation * WORLD_UP;
  return glm::lookAt(position, position + forward, up);
}
auto Light::SetTransform(const Transform &transform) -> void {
  position = transform.position;
  rotation = transform.rotation;
}
} // namespace kuki
