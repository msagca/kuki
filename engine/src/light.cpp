#define GLM_ENABLE_EXPERIMENTAL
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/quaternion_float.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/gtx/quaternion.hpp>
#include <light.hpp>
#include <transform.hpp>
namespace kuki {
auto Light::GetTransform() const -> Transform {
  Transform transform;
  transform.position = position;
  transform.rotation = rotation;
  transform.local = glm::translate(glm::mat4(1.f), position) * glm::toMat4(rotation);
  transform.world = transform.local;
  return transform;
}
auto Light::GetView() const -> glm::mat4 {
  return glm::lookAt(position, position + forward, up);
}
auto Light::SetRotation(const glm::quat &rotation) -> void {
  this->rotation = rotation;
  auto R = glm::toMat4(rotation);
  right = glm::vec3(R[0]);
  up = glm::vec3(R[1]);
  forward = -glm::vec3(R[2]);
}
auto Light::SetTransform(const Transform &transform) -> void {
  position = transform.position;
  SetRotation(transform.rotation);
}
} // namespace kuki
