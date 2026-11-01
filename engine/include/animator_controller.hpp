#pragma once
#include <asset.hpp>
#include <id.hpp>
#include <kuki_engine_export.h>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
namespace kuki {
struct ModelAsset;
/// @brief Something a state says on one of its ticks, by name, for the game to act on.
///
/// The tick is counted from the state being entered, in the application's fixed ticks and not in
/// the clip's own time, so it means the same moment on every machine however the clip is sampled.
/// In a state that loops it is counted from the start of each time round. A state's first 32
/// events can fire; the animator records which in a mask (`Animator::fired`).
struct AnimationEvent {
  int tick{};
  std::string name;
};
/// @brief One of the clips a blended state mixes, and the value at which it is the whole of the mix.
///
/// A blend lays its clips out along a number -- how fast, usually -- and plays whichever two the
/// animator's value falls between, mixed by how far along it is from one to the other. So a walk at
/// 1 and a run at 4 make a jog at 2.5. `source` and `clip` name the clip as a state's own do.
struct BlendClip {
  AssetID source{};
  std::string clip;
  float at{};
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
  /// @brief Clips to mix in place of the one above, by the animator's `blend`. Empty for a state
  /// that plays one clip.
  ///
  /// With any here the state is a blend: `source` and `clip` are not used, and what plays is the
  /// two of these that the value lies between. All of them are played at one shared pace -- a cycle
  /// lasts as long as the mix of the two clips' own lengths -- so that the same point of the stride
  /// is showing in both and nothing slides. That is only as good as the clips agree about where a
  /// cycle starts: every one should be a single cycle beginning on the same foot.
  ///
  /// An event's tick in a blend is counted from entering the state and not round each cycle, since
  /// a cycle is no fixed number of ticks when its length depends on the value.
  std::vector<BlendClip> blend;
};
/// @brief Where a controller's clip is in one model, found once: the clip's index, and for each of
/// its channels the node of the model it moves, or -1.
///
/// For a clip of the model's own the nodes repeat what each channel says; for one from another
/// model they are the match by name, done once rather than on every frame.
struct ClipBinding {
  int clipIndex{-1};
  std::vector<int> nodes;
};
/// @brief A state's clips as found in one model: its own, and each of its blend's.
struct StateBinding {
  ClipBinding clip;
  std::vector<ClipBinding> blend;
};
/// @brief The states an animator can be in, kept once and shared by every animator that plays them.
///
/// An asset rather than part of the animator, so that the animator is plain data -- trivially
/// copyable, a row of numbers -- and so that two fighters on the same moves hold one machine
/// between them rather than a copy each. An animator names it by id (`Animator::controller`) and
/// keeps only where it is in it: which state, how many ticks in, what last fired.
///
/// There are deliberately no conditions or parameters for transitions to be written in: what a
/// character may do next, and when, is a rule of the game, and a rule kept in two places is kept
/// in neither. A state goes on to its `next` when its clip ends; anything else is the game's `Play`.
struct KUKI_ENGINE_API AnimatorController final : public Asset {
  explicit AnimatorController(AssetID = AssetID::Invalid);
  std::vector<AnimationState> states;
  /// @brief The node whose travel across the ground is taken out of the pose and handed to the game.
  ///
  /// A walk made by an animator moves forward in the clip. Played as it is, the character strides
  /// away from its entity and snaps back when the clip comes round. Named here, that node is held
  /// where the clip starts it and its movement each tick is reported in `Animator::rootDelta`, for
  /// the game to apply to the entity -- or not to, at a wall. Along the ground only: height stays in
  /// the pose, where a jump is easier to author.
  ///
  /// Empty leaves the clip alone.
  std::string rootNode;
  /// @brief The state by that name, or -1.
  auto FindState(const std::string_view) const -> int;
  /// @brief Where this controller's clips are in a model, worked out the first time it is asked.
  ///
  /// Kept per model, since animators on different models share a controller and a clip's index
  /// and its channels' nodes are each model's own. Grown to the number of states as it is asked
  /// for, so a state added since is found too.
  auto Bind(const ModelAsset &) const -> std::vector<StateBinding> &;
  /// @brief Forgets where every clip was found, for an edit that changes which clip a state plays.
  auto Unbind() const -> void;
private:
  mutable std::unordered_map<AssetID, std::vector<StateBinding>> bindings;
};
} // namespace kuki
