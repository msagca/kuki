#include <application.hpp>
#include <physics_system.hpp>
#include <profiler.hpp>
#include <system.hpp>
#include <transform.hpp>
#include <utility>
namespace kuki {
PhysicsSystem::PhysicsSystem(Application &app)
  : System(std::in_place_type<PhysicsSystem>, app) {}
auto PhysicsSystem::Start() -> void {}
auto PhysicsSystem::Tick() -> void {}
auto PhysicsSystem::Update(const float) -> void {
  KUKI_PROFILE_SCOPE("Physics");
  auto scene = app.GetScene();
  if (!scene)
    return;
  scene->UpdateComponents<Transform>();
}
auto PhysicsSystem::Shutdown() -> void {}
} // namespace kuki
