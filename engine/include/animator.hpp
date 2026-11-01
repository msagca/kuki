#pragma once
#include <cstdint>
#include <fixed_string.hpp>
#include <fixed_vector.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/gtc/quaternion.hpp>
#include <id.hpp>
#include <kuki_engine_export.h>
#include <string_view>
namespace kuki {
struct AnimatorController;
/// @brief What an inverse kinematics goal does with the bones it is given.
///
/// `Reach` takes three: a bone, its parent and its parent's parent -- a foot, a shin and a thigh, or
/// a hand, a forearm and an upper arm -- and bends the two upper ones so the first lands on a point.
/// `Aim` turns a bone, and as many of its parents as are asked to help, so that it faces a point.
enum class IKKind {
  Reach,
  Aim,
};
/// @brief A point a bone is to get to or to face, solved after the pose has been sampled.
///
/// Clips are made once and played everywhere, and the world they are played in is never the one
/// they were made in: a floor is higher on one side, an opponent is shorter than the one the
/// punch was made against, a partner's hand is not where the clip left room for it. A goal is the
/// correction for that, made on every frame on top of whatever the clips said. It changes nothing
/// else: the pose is still the clip's above the bones it turns and below them, which follow along.
///
/// It is drawing, not rules. A goal is solved in `AnimationSystem::LateUpdate`, after every system
/// has moved what it is going to move this frame, and nothing a tick reads may depend on it.
///
/// The point is `offset`, either in the world or, with a `target`, in that entity's own space so
/// that it follows it. A target can be any entity at all, the bone of another skeleton included.
struct IKGoal {
  /// @brief What the game calls the goal by, to find it with `FindGoal`.
  FixedString<32> name;
  IKKind kind{IKKind::Reach};
  /// @brief The node of the animator's model that is to arrive at the point or to face it.
  FixedString<64> bone;
  EntityID target{EntityID::Invalid};
  glm::vec3 offset{};
  /// @brief How much of the correction is made, from none of it to all. The way to bring one in.
  float weight{1.f};
  /// @brief Reach: the way the middle joint should point, in the animator entity's own axes.
  ///
  /// A knee forwards and an elbow back and down. Without one the joint keeps bending whichever way
  /// the clip bent it, which is right until the limb is held straight and the way is lost.
  glm::vec3 bend{};
  /// @brief Reach: whether the bone keeps the turn the pose gave it in the world.
  ///
  /// A foot set down stays flat however the leg above it has been bent to put it there. Without
  /// this it turns with the shin, which is right for a fist at the end of a punch.
  bool holdRotation{true};
  /// @brief Aim: the direction that is to face the point, in the animator entity's own axes as the
  /// model was built. Forward, for a head, whatever way the head's own bone happens to lie.
  glm::vec3 axis{.0f, .0f, 1.f};
  /// @brief Aim: how many bones share the turn, counting up from `bone`. A head turned by the neck
  /// and the chest as well as by itself turns as a person does, and not as an owl.
  int chain{1};
  /// @brief Aim: the most it will turn, in degrees. A point behind is looked at over the shoulder.
  float limit{90.f};
};
/// @brief A bone's rotation as the pose left it and as the goals left it, kept until the next frame.
///
/// A goal is a correction to the pose, and a correction to a correction is something else. When a
/// bone the goals turned is still as they left it at the next frame -- no clip moves it, so nothing
/// put it back -- it is returned to the pose before the goals are solved again. Otherwise every
/// frame would start from the last one's answer, and a goal at half weight would go half of the
/// rest of the way each time, and soon all of it.
struct IKHeld {
  EntityID bone{EntityID::Invalid};
  glm::quat posed{1.f, .0f, .0f, .0f};
  glm::quat solved{1.f, .0f, .0f, .0f};
};
struct BonePose {
  glm::vec3 position{};
  glm::quat rotation{1.f, .0f, .0f, .0f};
  glm::vec3 scale{1.f};
  /// @brief Whether anything has been written here. A node no clip has touched has no pose to blend from.
  bool set{};
};
/// @brief How many goals one animator can have. A character reaching with both hands, planting both
/// feet and watching something with its head is five.
inline constexpr size_t MAX_IK_GOALS = 6;
/// @brief Plays a model's clips on the skeleton beside it.
///
/// Two ways, and which one depends on whether it has a `controller` with any states.
///
/// With none it is what it always was: one clip by index, advanced by the frame's own length. That
/// is the editor's preview, and what a model dropped into a scene does by default.
///
/// With states it is a small machine stepped by the fixed tick (`StepAnimator`), whose states are
/// the controller's and shared with every animator on the same moves. It is in one state at a time;
/// a state plays a clip, fires its events on the ticks they name, and at the end of a clip that does
/// not loop goes on to its `next`. Anything else that changes the state is the game calling `Play`.
///
/// What a tick decides and what a frame draws are kept apart. `state`, `stateTicks`, `fired` and
/// `rootDelta` are the first kind: whole numbers of ticks, the same on every machine. The pose is
/// the second: sampled in `Update`, between two ticks, blended across a fade, and kept in the
/// model's `Rig` since it is as long as the model has nodes. Nothing a tick reads depends on it.
///
/// Plain data, trivially copyable: everything that varies in length is the controller's or the
/// rig's, so the animator can be copied, snapshotted and rolled back as the bytes it is.
struct Animator {
  AssetID modelAssetId{};
  /// @brief The states it can be in (`AnimatorController`). None plays one clip by frame time.
  AssetID controller{};
  int clipIndex{-1};
  float time{};
  bool playing{true};
  bool loop{true};
  int state{-1};
  /// @brief How many ticks the current state has been stepped.
  int stateTicks{};
  /// @brief Whether a state with no `next` has reached the end of its clip.
  bool finished{};
  /// @brief The state whose events the last tick fired, and which of them: bit `i` is its `i`th
  /// event. Replaced every tick; asked of with `Fired`.
  int firedState{-1};
  uint32_t fired{};
  /// @brief How far the root node travelled during the last tick, in the entity's own axes.
  ///
  /// `position += rotation * rootDelta` moves the entity as the clip meant.
  glm::vec3 rootDelta{};
  /// @brief How many times a state has been entered. Counted so that drawing can tell a state began
  /// since the last frame, and fade into it from the pose that was showing (`Rig::entries`).
  uint32_t entries{};
  int fadeTicks{};
  /// @brief How far through its cycle a blended state is, from 0 up to 1, and where it was a tick ago.
  float phase{};
  float previousPhase{};
  /// @brief Where along its blend the current state is (`AnimationState::blend`). The game sets it.
  float blend{};
  /// @brief Points for bones to reach or to face, solved after the pose from the top of the
  /// skeleton down, so that no goal moves a bone another has already put in place. See `IKGoal`.
  FixedVector<IKGoal, MAX_IK_GOALS> ik;
};
/// @brief Enters a state by its place in the controller, from its first tick.
///
/// The change is immediate as far as the tick is concerned -- `state` is the new one when this
/// returns -- and gradual as far as the eye is: the pose showing at this moment is kept and faded
/// out of. Kept rather than left playing, so that leaving a state part way through its own fade
/// starts from what is on screen and nothing jumps.
KUKI_ENGINE_API auto Enter(Animator &, const AnimatorController &, const int) -> void;
/// @brief Enters a state by name. Answers false if there is none by that name.
///
/// Asking for the state already playing does nothing unless told to restart it, so a game can
/// say "walk" on every tick the key is down without the walk beginning again each time.
KUKI_ENGINE_API auto Play(Animator &, const AnimatorController &, const std::string_view, const bool restart = false) -> bool;
/// @brief Whether the last tick fired an event by that name.
KUKI_ENGINE_API auto Fired(const Animator &, const AnimatorController &, const std::string_view) -> bool;
/// @brief The name of the state the animator is in, or empty.
KUKI_ENGINE_API auto GetStateName(const Animator &, const AnimatorController &) -> std::string_view;
/// @brief The goal by that name, or null.
KUKI_ENGINE_API auto FindGoal(Animator &, const std::string_view) -> IKGoal *;
} // namespace kuki
