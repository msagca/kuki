#pragma once
#include <scene_manager.hpp>
#include <system.hpp>
namespace kuki {
class KUKI_ENGINE_API PhysicsSystem final : public System {
public:
  PhysicsSystem(SceneManager &);
  ~PhysicsSystem();
  auto Awake() -> void override;
  auto Start() -> void override;
  auto Update(const float) -> void override;
  auto Shutdown() -> void override;
private:
  SceneManager &sceneManager;
  const float simulationTimestep;
  float timeAccumulated{};
  int cameraDirty;
};
} // namespace kuki
