#pragma once
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/gtc/quaternion.hpp>
#include <id.hpp>
#include <kuki_engine_export.h>
#include <ostream>
namespace kuki {
/// @brief Where an entity is relative to its parent: what a game or the editor writes.
///
/// The world matrix this works out to is a component of its own, `WorldMatrix`, kept in a column
/// beside this one. The two were one struct, and the struct was some 180 bytes of which a renderer
/// wants 64: an archetype's world matrices sat that far apart, so handing a pool of them to the GPU
/// was a gather and not a copy. Apart, a pool's world matrices are one block of memory in the order
/// its entities are in. Every archetype with a `Transform` is given a `WorldMatrix` (see `Archetype`).
struct KUKI_ENGINE_API Transform {
  glm::vec3 position{};
  glm::quat rotation{};
  glm::vec3 scale{1.0f};
  EntityID parent{EntityID::Invalid};
  glm::mat4 local{1.0f};
  /// @brief Copies the transform's local state.
  ///
  /// Deliberately does not copy the parent ID, so assigning to a transform keeps it attached to its current parent.
  ///
  /// Does not mark anything dirty, because a transform no longer carries that bit: `EntityManager`
  /// keeps it, packed, beside the update order.
  Transform &operator=(const Transform &);
  /// @brief Works `local` out from the position, rotation and scale.
  void Update();
  /// @brief Takes a new parent, or none, whose world matrix is `parent`.
  ///
  /// `world` is this entity's own world matrix, which is read when the transform keeps where it is
  /// in the world (`keepWorld`, or no parent at all) and written with where it ends up.
  void Reparent(const glm::mat4 *parent, glm::mat4 &world, bool keepWorld = false);
};
/// @brief Where an entity is in the world, worked out from its `Transform` and its parents'.
///
/// Written by the transform update and by nothing else that means it: a change made here is undone
/// the next time the entity or a parent moves. Read it to know where something is.
struct WorldMatrix {
  glm::mat4 matrix{1.0f};
};
std::ostream &operator<<(std::ostream &, const glm::mat4 &);
} // namespace kuki
