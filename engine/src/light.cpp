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
  const auto translation = glm::translate(glm::mat4(1.f), transform.position);
  transform.local = translation * glm::toMat4(transform.rotation);
  return transform;
}
auto Light::SetTransform(const Transform &transform) -> void {
  position = transform.position;
  rotation = transform.rotation;
}
} // namespace kuki
