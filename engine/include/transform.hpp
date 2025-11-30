#pragma once
#include <component.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/gtc/quaternion.hpp>
#include <id.hpp>
#include <kuki_engine_export.h>
#include <ostream>
namespace kuki {
struct KUKI_ENGINE_API Transform final : public IComponent {
  Transform();
  glm::vec3 position{};
  glm::quat rotation{};
  glm::vec3 scale{1.0f};
  ID parent{ID::Invalid()};
  glm::mat4 local{1.0f};
  glm::mat4 world{1.0f};
  // TODO: currently, the only consumer of this dirty flag is the rendering system; consider replacing it with a generational counter later
  bool dirty{true};
  Transform& operator=(const Transform&);
  /// @param parent Parent transform (can be `NULL`)
  void Update(const Transform* = nullptr);
  /// @param parent Parent transform (can be `NULL`)
  /// @param keepWorld Preserve child's world transform (`true` if parent is `NULL`)
  void Reparent(const Transform* = nullptr, bool = false);
};
std::ostream& operator<<(std::ostream&, const glm::mat4&);
} // namespace kuki
