#pragma once
#include <kuki_engine_export.h>
#include <system.hpp>
namespace kuki {
struct Animator;
struct ModelAsset;
/// @brief Steps an animator that has states by one tick: events, root travel, and the end of a clip.
///
/// Everything a game's rules may read about an animation is settled here and nowhere else, from
/// whole numbers of ticks. Free of the application so it can be run on its own -- by a test, or
/// again over ticks that have to be replayed.
///
/// `rootDelta` is left in the space of the root node's parent. `AnimationSystem::Tick` is what
/// turns it to the entity's own axes, since that takes the scene.
KUKI_ENGINE_API auto StepAnimator(Animator &, const ModelAsset &, const int) -> void;
/// @brief Fills an animator's pose for drawing, `alpha` of the way from the last tick to the next.
KUKI_ENGINE_API auto PoseAnimator(Animator &, const ModelAsset &, const int, const float) -> void;
class KUKI_ENGINE_API AnimationSystem final : public System {
public:
  AnimationSystem(Application &);
  void Start() override;
  /// @brief Steps every animator that has states. Put this system ahead of `ScriptingSystem` in an
  /// application's list for a script to see a tick's events during that same tick.
  auto Tick() -> void override;
  void Update(const float) override;
  void Shutdown() override;
};
} // namespace kuki
