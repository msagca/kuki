#pragma once
#include "system.hpp"
namespace kuki {
class Application;
class KUKI_ENGINE_API PhysicsSystem final : public System {
private:
  float timeAccumulated{};
  const float simulationTimestep;
public:
  PhysicsSystem(Application&);
  ~PhysicsSystem();
  void Start() override;
  void Update(float) override;
  void Shutdown() override;
};
} // namespace kuki
