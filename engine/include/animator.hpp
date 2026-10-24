#pragma once
#include <glm/ext/vector_float3.hpp>
#include <glm/gtc/quaternion.hpp>
#include <id.hpp>
#include <kuki_engine_export.h>
#include <string>
#include <string_view>
#include <vector>
namespace kuki {
/// @brief Something a state says on one of its ticks, by name, for the game to act on.
///
/// The tick is counted from the state being entered, in the application's fixed ticks and not in
/// the clip's own time, so it means the same moment on every machine however the clip is sampled.
/// In a state that loops it is counted from the start of each time round.
struct AnimationEvent {
  int tick{};
  std::string name;
};
/// @brief One of the clips a blended state mixes, and the value at which it is the whole of the mix.
///
/// A blend lays its clips out along a number -- how fast, usually -- and plays whichever two the
/// state's value falls between, mixed by how far along it is from one to the other. So a walk at 1
/// and a run at 4 make a jog at 2.5. `source` and `clip` name the clip as a state's own do.
struct BlendClip {
  AssetID source{};
  std::string clip;
  float at{};
  int clipIndex{-1};
  std::vector<int> nodes;
};
/// @brief One thing an animator can be doing: a clip, how it is played, and what follows it.
struct AnimationState {
  std::string name;
  /// @brief The model the clip is in, where that is not the animator's own.
  ///
  /// A character and its clips are often separate files: one with the mesh and the skeleton, and
  /// one a clip with only the skeleton in it. Named here, the clip is taken from that other model
  /// and played on this one's skeleton, bone for bone by name -- the two files do not have to list
  /// their nodes in the same order, only to call them the same things. A bone the clip names and
  /// this skeleton lacks is skipped.
  ///
  /// Left empty, the clip is one of the animator's own model's.
  AssetID source{};
  /// @brief The clip's name in the model it comes from. A name as an exporter prefixes it --
  /// `Armature|Punch` -- is matched by the part after the bar as well.
  ///
  /// Empty takes the model's first clip, which is the only way to ask for one when a tool has given
  /// every clip it exports the same name. Then the file is what tells them apart.
  std::string clip;
  bool loop{};
  float speed{1.f};
  /// @brief How many ticks the pose takes to arrive from whatever was showing when this was entered.
  int fade{6};
  /// @brief The state entered when this one's clip runs out. Empty holds the last pose instead.
  std::string next;
  std::vector<AnimationEvent> events;
  /// @brief Clips to mix in place of the one above, by `value`. Empty for a state that plays one clip.
  ///
  /// With any here the state is a blend: `source` and `clip` are not used, and what plays is the
  /// two of these that `value` lies between. All of them are played at one shared pace -- a cycle
  /// lasts as long as the mix of the two clips' own lengths -- so that the same point of the stride
  /// is showing in both and nothing slides. That is only as good as the clips agree about where a
  /// cycle starts: every one should be a single cycle beginning on the same foot.
  ///
  /// An event's tick in a blend is counted from entering the state and not round each cycle, since
  /// a cycle is no fixed number of ticks when its length depends on the value.
  std::vector<BlendClip> blend;
  /// @brief Where along its blend this state is. The game sets it; nothing here changes it.
  float value{};
  /// @brief The clip's index in the model, looked up from the name the first time it is wanted.
  int clipIndex{-1};
  /// @brief For each of the clip's channels, the node of the animator's model it moves, or -1.
  ///
  /// Worked out with the index above and kept beside it. For a clip of the model's own this repeats
  /// what each channel already says; for one from another model it is the matching by name, done
  /// once rather than on every frame.
  std::vector<int> nodes;
};
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
/// It is drawing, not rules. A goal is solved in `AnimationSystem::LateUpdate`, after every script
/// has moved what it is going to move this frame, and nothing a tick reads may depend on it.
///
/// The point is `offset`, either in the world or, with a `target`, in that entity's own space so
/// that it follows it. A target can be any entity at all, the bone of another skeleton included.
struct IKGoal {
  /// @brief What the game calls the goal by, to find it with `Animator::FindGoal`.
  std::string name;
  IKKind kind{IKKind::Reach};
  /// @brief The node of the animator's model that is to arrive at the point or to face it.
  std::string bone;
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
/// @brief Plays a model's clips on the skeleton beside it.
///
/// Two ways, and which one depends on whether there are any states.
///
/// With none it is what it always was: one clip by index, advanced by the frame's own length. That
/// is the editor's preview, and what a model dropped into a scene does by default.
///
/// With states it is a small machine stepped by the fixed tick. It is in one state at a time; a
/// state plays a clip, fires its events on the ticks they name, and at the end of a clip that does
/// not loop goes on to its `next`. Anything else that changes the state is the game calling `Play`.
/// There are deliberately no conditions or parameters for transitions to be written in: what a
/// character may do next, and when, is a rule of the game, and a rule kept in two places is kept
/// in neither.
///
/// What a tick decides and what a frame draws are kept apart. `state`, `stateTicks`, `events` and
/// `rootDelta` are the first kind: whole numbers of ticks, the same on every machine. The pose is
/// the second: sampled in `Update`, between two ticks, blended across a fade. Nothing a tick reads
/// depends on it.
struct KUKI_ENGINE_API Animator {
  AssetID modelAssetId{};
  int clipIndex{-1};
  float time{};
  bool playing{true};
  bool loop{true};
  std::vector<AnimationState> states;
  /// @brief The node whose travel across the ground is taken out of the pose and handed to the game.
  ///
  /// A walk made by an animator moves forward in the clip. Played as it is, the character strides
  /// away from its entity and snaps back when the clip comes round. Named here, that node is held
  /// where the clip starts it and its movement each tick is reported in `rootDelta`, for the game to
  /// apply to the entity -- or not to, at a wall. Along the ground only: height stays in the pose,
  /// where a jump is easier to author.
  ///
  /// Empty leaves the clip alone.
  std::string rootNode;
  int state{-1};
  /// @brief How many ticks the current state has been stepped.
  int stateTicks{};
  /// @brief Whether a state with no `next` has reached the end of its clip.
  bool finished{};
  /// @brief The names of the events the last tick fired. Replaced every tick.
  std::vector<std::string> events;
  /// @brief How far the root node travelled during the last tick, in the entity's own axes.
  ///
  /// `position += rotation * rootDelta` moves the entity as the clip meant.
  glm::vec3 rootDelta{};
  std::vector<BonePose> pose;
  std::vector<BonePose> fadeFrom;
  int fadeTicks{};
  /// @brief How far through its cycle a blended state is, from 0 up to 1, and where it was a tick ago.
  float phase{};
  float previousPhase{};
  /// @brief Points for bones to reach or to face, solved after the pose from the top of the
  /// skeleton down, so that no goal moves a bone another has already put in place. See `IKGoal`.
  std::vector<IKGoal> ik;
  std::vector<IKHeld> ikHeld;
  /// @brief Enters a state by name. Answers false if there is none by that name.
  ///
  /// Asking for the state already playing does nothing unless told to restart it, so a game can
  /// say "walk" on every tick the key is down without the walk beginning again each time.
  ///
  /// The change is immediate as far as the tick is concerned -- `state` is the new one when this
  /// returns -- and gradual as far as the eye is: the pose showing at this moment is kept and faded
  /// out of. Kept rather than left playing, so that leaving a state part way through its own fade
  /// starts from what is on screen and nothing jumps.
  auto Play(const std::string_view, const bool = false) -> bool;
  /// @brief Sets where along its blend a state is, by the state's name. False if there is none.
  auto SetBlend(const std::string_view, const float) -> bool;
  auto Fired(const std::string_view) const -> bool;
  /// @brief The goal by that name, or null.
  auto FindGoal(const std::string_view) -> IKGoal *;
  auto FindState(const std::string_view) const -> int;
  auto GetStateName() const -> std::string_view;
  auto Enter(const int) -> void;
};
} // namespace kuki
