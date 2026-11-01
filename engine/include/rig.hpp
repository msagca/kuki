#pragma once
#include <animator.hpp>
#include <cstdint>
#include <id.hpp>
#include <vector>
namespace kuki {
/// @brief What a placed model with a skeleton has that varies in length, kept by its scene.
///
/// Reached from the model's root through its `Skeleton`, which holds only the slot (`Scene::GetRig`).
/// The scene keeps it rather than `AnimationSystem` because more than animation reads it: the
/// renderer skins meshes from `nodes` in applications with no animation system at all, and saving a
/// scene puts back what the inverse kinematics turned.
struct Rig {
  /// @brief The entity standing for each of the model's nodes, by the node's index; invalid where none.
  std::vector<EntityID> nodes;
  /// @brief The pose last drawn, one per node, and the pose a state entered since then fades out of.
  std::vector<BonePose> pose;
  std::vector<BonePose> fadeFrom;
  /// @brief The second clip of a blend, sampled here rather than into a buffer made every frame.
  std::vector<BonePose> scratch;
  /// @brief The bones the goals turned, with the turn the pose gave each and the one they left. See `IKHeld`.
  std::vector<IKHeld> held;
  /// @brief The animator's `entries` when the pose was last drawn.
  ///
  /// When the animator's count has moved on, a state has been entered since, and the pose showing
  /// is taken as the one to fade out of before the new state's is drawn over it. Looked at when
  /// drawing rather than done by `Enter`, which has no pose to hand: it is the tick's, and the pose
  /// is the frame's.
  uint32_t entries{};
};
} // namespace kuki
