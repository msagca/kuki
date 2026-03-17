#include <physics_system.hpp>
#include <system.hpp>
namespace kuki {
PhysicsSystem::PhysicsSystem()
  : System(std::in_place_type<PhysicsSystem>), simulationTimestep(1.f / 100) {}
PhysicsSystem::~PhysicsSystem() {
  Shutdown();
}
auto PhysicsSystem::Awake() -> void {}
auto PhysicsSystem::Start() -> void {
  timeAccumulated = 0.f;
}
auto PhysicsSystem::Update(const float deltaTime) -> void {
  timeAccumulated += deltaTime;
  while (timeAccumulated > simulationTimestep) {
    // run the physics simulation
    timeAccumulated -= simulationTimestep;
  }
  const auto alpha = timeAccumulated / simulationTimestep;
  // TODO: use this value to update the state via linear interpolation to fix visual stuttering
}
auto PhysicsSystem::Shutdown() -> void {}
} // namespace kuki
