#include <component/component.hpp>
#include <component/light.hpp>
#include <component/transform.hpp>
#include <glm/ext/quaternion_float.hpp>
#include <glm/ext/vector_float3.hpp>
namespace kuki {
Transform Light::GetTransform() const {
  static const auto WORLD_UP = glm::vec3(.0f, 1.0f, .0f);
  static const auto WORLD_BACK = glm::vec3(.0f, .0f, 1.0f);
  static const auto PARALLEL_THRESHOLD = .9999f;
  Transform transform;
  if (type == LightType::Directional) {
    transform.position = glm::vec3(.0f);
    auto direction = glm::normalize(vector);
    auto up = WORLD_UP;
    if (glm::abs(glm::dot(direction, up)) > PARALLEL_THRESHOLD)
      up = WORLD_BACK;
    transform.rotation = glm::quatLookAt(direction, up);
  } else if (type == LightType::Point) {
    transform.position = vector;
    transform.rotation = glm::quat(1.0f, .0f, .0f, .0f);
  }
  return transform;
}
void Light::SetTransform(const Transform& transform) {
  if (type == LightType::Directional) {
    auto forward = glm::vec3(.0f, .0f, -1.0f);
    vector = glm::normalize(transform.rotation * forward);
  } else if (type == LightType::Point)
    vector = transform.position;
}
} // namespace kuki
