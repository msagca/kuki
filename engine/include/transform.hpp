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
  /// @brief Copies the transform's local and world state.
  ///
  /// Deliberately does not copy the parent ID, so assigning to a transform keeps it attached to its current parent.
  ///
  /// Does not mark anything dirty, because a transform no longer carries that bit: `EntityManager`
  /// keeps it, packed, beside the update order. Every caller reaches this through `AddComponent`,
  /// which moves the entity between archetypes and so already forces the next update to rebuild.
  Transform &operator=(const Transform &);
  void Update(const Transform * = nullptr);
  void Reparent(const Transform * = nullptr, bool = false);
};
std::ostream &operator<<(std::ostream &, const glm::mat4 &);
} // namespace kuki
