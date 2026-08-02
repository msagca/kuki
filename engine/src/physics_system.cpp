#include <application.hpp>
#include <physics_system.hpp>
#include <system.hpp>
#include <transform.hpp>
#include <utility>
namespace kuki {
PhysicsSystem::PhysicsSystem(Application &app)
  : System(std::in_place_type<PhysicsSystem>, app), simulationTimestep(1.f / 100) {}
auto PhysicsSystem::Start() -> void {
  timeAccumulated = 0.f;
}
auto PhysicsSystem::Update(const float deltaTime) -> void {
  timeAccumulated += deltaTime;
  while (timeAccumulated > simulationTimestep) {
    timeAccumulated -= simulationTimestep;
  }
  const auto alpha = timeAccumulated / simulationTimestep;
  // TODO: use this value to update the state via linear interpolation to fix visual stuttering
  auto scene = app.GetScene();
  if (!scene)
    return;
  scene->UpdateComponents<Transform>();
}
auto PhysicsSystem::Shutdown() -> void {}
} // namespace kuki
