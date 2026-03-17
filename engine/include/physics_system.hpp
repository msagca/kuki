#pragma once
#include <system.hpp>
namespace kuki {
class KUKI_ENGINE_API PhysicsSystem final : public System {
public:
  PhysicsSystem();
  ~PhysicsSystem();
  auto Awake() -> void override;
  auto Start() -> void override;
  auto Update(const float) -> void override;
  auto Shutdown() -> void override;
private:
  float timeAccumulated{};
  const float simulationTimestep;
};
} // namespace kuki
