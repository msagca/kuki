#pragma once
#include <scene_manager.hpp>
#include <system.hpp>
namespace kuki {
class KUKI_ENGINE_API PhysicsSystem final : public System {
public:
  PhysicsSystem(Application &);
  auto Start() -> void override;
  /// @brief Flushes transform changes to the active scene.
  auto Update(const float) -> void override;
  /// @brief Where the simulation will be stepped. There is none yet.
  ///
  /// This system used to keep an accumulator of its own, at a hundred steps a second, with nothing
  /// inside the loop. The application's tick replaces it: one fixed step for everything that has
  /// rules, so that a script and the simulation it pushes on cannot be stepping at different rates.
  auto Tick() -> void override;
  auto Shutdown() -> void override;
};
} // namespace kuki
