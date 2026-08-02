#pragma once
#include <scene_manager.hpp>
#include <system.hpp>
namespace kuki {
class KUKI_ENGINE_API PhysicsSystem final : public System {
public:
  PhysicsSystem(Application &);
  auto Start() -> void override;
  auto Update(const float) -> void override;
  auto Shutdown() -> void override;
private:
  const float simulationTimestep;
  float timeAccumulated{};
};
} // namespace kuki
