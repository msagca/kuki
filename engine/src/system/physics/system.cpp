#include <application.hpp>
#include <system/physics.hpp>
#include <system/system.hpp>
#include <spdlog/spdlog.h>
namespace kuki {
PhysicsSystem::PhysicsSystem(Application& app)
  : System(app), simulationTimestep(1.0f / 100) {}
PhysicsSystem::~PhysicsSystem() {
  Shutdown();
}
void PhysicsSystem::Start() {
  timeAccumulated = .0f;
}
void PhysicsSystem::Update(float deltaTime) {
  timeAccumulated += deltaTime;
  while (timeAccumulated > simulationTimestep) {
    // run the physics simulation
    timeAccumulated -= simulationTimestep;
  }
  const auto alpha = timeAccumulated / simulationTimestep;
  // use this value to update the state via linear interpolation to fix visual stuttering
}
void PhysicsSystem::Shutdown() {
}
} // namespace kuki
