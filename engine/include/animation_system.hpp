#pragma once
#include <functional>
#include <id.hpp>
#include <kuki_engine_export.h>
#include <system.hpp>
#include <tick_clock.hpp>
namespace kuki {
struct Animator;
struct ModelAsset;
/// @brief How the animation code asks for a model it was not handed: by id, null if there is none.
using ModelLookup = std::function<const ModelAsset *(const AssetID)>;
/// @brief Steps an animator that has states by one tick: events, root travel, and the end of a clip.
///
/// Everything a game's rules may read about an animation is settled here and nowhere else, from
/// whole numbers of ticks. Free of the application so it can be run on its own -- by a test, or
/// again over ticks that have to be replayed.
///
/// `rootDelta` is left in the space of the root node's parent. `AnimationSystem::Tick` is what
/// turns it to the entity's own axes, since that takes the scene.
///
/// The last argument finds a model by id, for a state whose clip is in another model than the
/// animator's own. Left out, such a state has no clip and holds still.
KUKI_ENGINE_API auto StepAnimator(Animator &, const ModelAsset &, const int, const ModelLookup & = {}) -> void;
/// @brief Fills an animator's pose for drawing, `alpha` of the way from the last tick to the next.
KUKI_ENGINE_API auto PoseAnimator(Animator &, const ModelAsset &, const int, const float, const ModelLookup & = {}) -> void;
class KUKI_ENGINE_API AnimationSystem final : public System {
public:
  AnimationSystem(Application &);
  void Start() override;
  /// @brief Steps every animator that has states. Put this system ahead of a game's own in an
  /// application's list for the game to see a tick's events during that same tick.
  auto Tick() -> void override;
  void Update(const float) override;
  /// @brief Solves every animator's inverse kinematics goals, on the pose `Update` sampled and with
  /// characters and targets where the game's systems have put them.
  auto LateUpdate(const float) -> void override;
  void Shutdown() override;
private:
  /// @brief Counts ticks for animators to be stepped by while the game is stopped.
  ///
  /// The tick is what steps an animator that has states, and a stopped application runs no ticks:
  /// in an editor, a character whose states were being set up would stand in its bind pose until
  /// the game was played. So while stopped, an animator marked as playing is stepped from here
  /// instead, at the same rate, for the sake of being looked at. Nothing reads what it reports --
  /// there is no game running to take an event or apply the root's travel -- and the count is
  /// forgotten the moment the game starts, which begins every state again from its first tick.
  TickClock preview;
};
} // namespace kuki
