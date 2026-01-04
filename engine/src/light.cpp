#define GLM_ENABLE_EXPERIMENTAL
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/quaternion_float.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/gtx/quaternion.hpp>
#include <light.hpp>
#include <transform.hpp>
#include <utility>
namespace kuki {
auto Light::GetTransform() const -> Transform {
  // TODO: cache the transform
  static const auto WORLD_UP = glm::vec3(0.f, 1.f, 0.f);
  static const auto WORLD_BACK = glm::vec3(0.f, 0.f, 1.f);
  static const auto PARALLEL_THRESHOLD = .9999f;
  Transform transform;
  if (type == LightType::Directional) {
    transform.position = glm::vec3(0.f);
    const auto forward = glm::normalize(vector);
    auto up = WORLD_UP;
    if (glm::abs(glm::dot(forward, up)) > PARALLEL_THRESHOLD)
      up = WORLD_BACK;
    transform.rotation = glm::quatLookAt(forward, up);
  } else if (type == LightType::Point) {
    transform.position = vector;
    transform.rotation = glm::quat(1.f, 0.f, 0.f, 0.f);
  }
  const auto translation = glm::translate(glm::mat4(1.f), transform.position);
  transform.local = translation * glm::toMat4(transform.rotation);
  return transform;
}
auto Light::SetTransform(const Transform &transform) -> void {
  if (type == LightType::Directional) {
    const auto forward = glm::vec3(0.f, 0.f, -1.f);
    vector = glm::normalize(transform.rotation * forward);
  } else if (type == LightType::Point)
    vector = transform.position;
}
} // namespace kuki
