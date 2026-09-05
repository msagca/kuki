#pragma once
#include <scene_manager.hpp>
#include <system.hpp>
namespace kuki {
class KUKI_ENGINE_API PhysicsSystem final : public System {
public:
  PhysicsSystem(Application &);
  auto Start() -> void override;
  /// @brief Advances the simulation in fixed timesteps and flushes transform changes to the active scene.
  ///
  /// TODO: use the leftover accumulator fraction to interpolate state and remove the visual stutter
  auto Update(const float) -> void override;
  auto Shutdown() -> void override;
private:
  const float simulationTimestep;
  float timeAccumulated{};
};
} // namespace kuki
