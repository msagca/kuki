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
  // Spelled out: glm's quaternion constructor is defaulted (the engine does not define
  // GLM_FORCE_CTOR_INIT), so `{}` would make it all zeros -- which `glm::toMat4` happens to turn
  // into the identity matrix, hiding it, while `rotation * v`, `slerp` and `normalize` give nonsense.
  glm::quat rotation{1.f, .0f, .0f, .0f};
  glm::vec3 scale{1.0f};
  EntityID parent{EntityID::Invalid};
  glm::mat4 local{1.0f};
  // No copy assignment of its own. It used to copy everything but `parent`, so that assigning a
  // transform kept it where it hung -- which made the component not trivially copyable, and made
  // a table that moved a row into a deleted entity's place leave the old entity's parent behind.
  // `EntityManager` keeps the hierarchy (`idToParent`) and sets `parent` from it wherever a row
  // comes from elsewhere; assigning a whole transform now copies the whole transform.
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
/// @brief The rotation that turns -Z, the way a camera and a light look, to face `direction`.
///
/// Turned about world up, unless the direction is nearly vertical: there that up gives out, and +Z
/// stands in. A direction of no length is no rotation.
KUKI_ENGINE_API auto LookRotation(const glm::vec3 &direction) -> glm::quat;
/// @brief Where a world matrix puts its origin.
KUKI_ENGINE_API auto WorldPosition(const glm::mat4 &) -> glm::vec3;
/// @brief Which way a world matrix turns -Z, at unit length whatever the scale.
///
/// The way a camera looks and a light shines. -Z for a matrix that flattens it to nothing.
KUKI_ENGINE_API auto WorldForward(const glm::mat4 &) -> glm::vec3;
/// @brief Which way a world matrix turns +Y, at unit length whatever the scale.
KUKI_ENGINE_API auto WorldUp(const glm::mat4 &) -> glm::vec3;
/// @brief The view matrix of something placed by a world matrix and looking down its -Z: the
/// inverse of the place, without its scale.
KUKI_ENGINE_API auto ViewFrom(const glm::mat4 &) -> glm::mat4;
std::ostream &operator<<(std::ostream &, const glm::mat4 &);
} // namespace kuki
