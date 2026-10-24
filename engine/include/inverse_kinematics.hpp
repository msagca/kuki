#pragma once
#include <glm/ext/vector_float3.hpp>
#include <glm/gtc/quaternion.hpp>
#include <id.hpp>
#include <kuki_engine_export.h>
namespace kuki {
class Application;
struct Animator;
struct ModelAsset;
struct Skeleton;
/// @brief The two turns that put the end of a two-bone limb on a point, both in world space.
///
/// `mid` is made first, at the middle joint, with the limb where it was; `root` after it, at the
/// top joint, and carries the whole limb -- the middle joint's turn with it. Bone lengths are kept,
/// since nothing is moved but turned.
struct TwoBoneTurn {
  glm::quat root{1.f, .0f, .0f, .0f};
  glm::quat mid{1.f, .0f, .0f, .0f};
};
/// @brief Solves a limb of two bones -- joints at `a`, `b` and `c` -- for `c` to arrive at `target`.
///
/// Worked out rather than searched for, and in three steps, each exact on its own. The middle joint
/// is opened or closed until its two ends are as far apart as the top joint is from the target;
/// the whole limb is swung at the top joint to lay that line on the target; and then the limb is
/// turned about that line until the middle joint is on the side of it `bend` points to. A target
/// out of reach is reached for in a straight line, held a hair short of straight so that which way
/// the joint bends is still there to be found next frame. One nearer than the limb can fold to is
/// reached for folded.
///
/// `bend` may be zero, which leaves the joint bending the way it already was.
KUKI_ENGINE_API auto SolveTwoBone(const glm::vec3 &a, const glm::vec3 &b, const glm::vec3 &c, const glm::vec3 &target, const glm::vec3 &bend) -> TwoBoneTurn;
/// @brief The turn that takes one direction to another, by no more than `limit` radians.
///
/// Past the limit it is the same turn, cut short: towards the wanted direction as far as allowed.
KUKI_ENGINE_API auto SolveAim(const glm::vec3 &direction, const glm::vec3 &wanted, const float limit) -> glm::quat;
/// @brief Solves an animator's goals and turns its skeleton's bones to suit, those higher up first.
///
/// Works on the bones' entities as the frame has left them, going up through their parents for
/// where they are in the world rather than trusting the world matrices, which were last brought up
/// to date before this frame's pose was written.
KUKI_ENGINE_API auto ApplyInverseKinematics(Application &, const EntityID, Animator &, const Skeleton &, const ModelAsset &) -> void;
/// @brief Puts back what the goals turned, wherever nothing else has moved it since.
///
/// The bones stay as the pose has them until the goals are next solved. For what must not see the
/// goals' work as if it were the pose: a scene being saved, which would otherwise keep the turn
/// of every bone no clip moves, and load it back as where that bone rests.
KUKI_ENGINE_API auto UndoInverseKinematics(Application &, Animator &) -> void;
} // namespace kuki
