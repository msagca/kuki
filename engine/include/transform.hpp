#pragma once
#include <glm/ext/vector_float3.hpp>
#include <glm/gtc/quaternion.hpp>
#include <id.hpp>
#include <kuki_engine_export.h>
#include <ostream>
namespace kuki {
struct KUKI_ENGINE_API Transform {
  glm::vec3 position{};
  glm::quat rotation{};
  glm::vec3 scale{1.0f};
  EntityID parent{EntityID::Invalid};
  glm::mat4 local{1.0f};
  glm::mat4 world{1.0f};
  bool dirty{true};
  Transform &operator=(const Transform &);
  void Update(const Transform * = nullptr);
  void Reparent(const Transform * = nullptr, bool = false);
};
std::ostream &operator<<(std::ostream &, const glm::mat4 &);
} // namespace kuki
