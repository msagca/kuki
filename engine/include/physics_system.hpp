#pragma once
#include <system.hpp>
namespace kuki {
class KUKI_ENGINE_API PhysicsSystem final : public System {
private:
  float timeAccumulated{};
  const float simulationTimestep;
public:
  PhysicsSystem();
  ~PhysicsSystem();
  void Start() override;
  void Update(float) override;
  void Shutdown() override;
};
} // namespace kuki
