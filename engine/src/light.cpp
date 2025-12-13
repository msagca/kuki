#define GLM_ENABLE_EXPERIMENTAL
#include <component.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/quaternion_float.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/gtx/quaternion.hpp>
#include <light.hpp>
#include <transform.hpp>
#include <utility>
namespace kuki {
Light::Light()
  : IComponent(std::in_place_type<Light>) {}
Light::Light(const Light& other)
  : IComponent(other), type(other.type), vector(other.vector), ambient(other.ambient), diffuse(other.diffuse), specular(other.specular), constant(other.constant), linear(other.linear), quadratic(other.quadratic) {}
Light& Light::operator=(const Light& other) {
  type = other.type;
  vector = other.vector;
  ambient = other.ambient;
  diffuse = other.diffuse;
  specular = other.specular;
  constant = other.constant;
  linear = other.linear;
  quadratic = other.quadratic;
  return *this;
}
Transform Light::GetTransform() const {
  // TODO: cache the transform
  static const auto WORLD_UP = glm::vec3(.0f, 1.0f, .0f);
  static const auto WORLD_BACK = glm::vec3(.0f, .0f, 1.0f);
  static const auto PARALLEL_THRESHOLD = .9999f;
  Transform transform;
  if (type == LightType::Directional) {
    transform.position = glm::vec3(.0f);
    auto forward = glm::normalize(vector);
    auto up = WORLD_UP;
    if (glm::abs(glm::dot(forward, up)) > PARALLEL_THRESHOLD)
      up = WORLD_BACK;
    transform.rotation = glm::quatLookAt(forward, up);
  } else if (type == LightType::Point) {
    transform.position = vector;
    transform.rotation = glm::quat(1.0f, .0f, .0f, .0f);
  }
  auto translation = glm::translate(glm::mat4(1.0f), transform.position);
  transform.local = translation * glm::toMat4(transform.rotation);
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
