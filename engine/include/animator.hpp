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
/// @brief One thing an animator can be doing: a clip, how it is played, and what follows it.
struct AnimationState {
  std::string name;
  /// @brief The clip's name in the model. A name as an exporter prefixes it -- `Armature|Punch` --
  /// is matched by the part after the bar as well.
  std::string clip;
  bool loop{};
  float speed{1.f};
  /// @brief How many ticks the pose takes to arrive from whatever was showing when this was entered.
  int fade{6};
  /// @brief The state entered when this one's clip runs out. Empty holds the last pose instead.
  std::string next;
  std::vector<AnimationEvent> events;
  /// @brief The clip's index in the model, looked up from the name the first time it is wanted.
  int clipIndex{-1};
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
  auto Fired(const std::string_view) const -> bool;
  auto FindState(const std::string_view) const -> int;
  auto GetStateName() const -> std::string_view;
  auto Enter(const int) -> void;
};
} // namespace kuki
