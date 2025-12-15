#include <application.hpp>
#include <physics_system.hpp>
#include <system.hpp>
namespace kuki {
PhysicsSystem::PhysicsSystem(Application& app)
  : System(app), simulationTimestep(1.f / 100) {}
PhysicsSystem::~PhysicsSystem() {
  Shutdown();
}
void PhysicsSystem::Start() {
  timeAccumulated = 0.f;
}
void PhysicsSystem::Update(float deltaTime) {
  timeAccumulated += deltaTime;
  while (timeAccumulated > simulationTimestep) {
    // run the physics simulation
    timeAccumulated -= simulationTimestep;
  }
  const auto alpha = timeAccumulated / simulationTimestep;
  // TODO: use this value to update the state via linear interpolation to fix visual stuttering
}
void PhysicsSystem::Shutdown() {
}
} // namespace kuki
