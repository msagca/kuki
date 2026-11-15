#include <application.hpp>
#include <glm/ext/quaternion_trigonometric.hpp>
#include <glm/ext/vector_float3.hpp>
#include <my_game_world.hpp>
#include <spin_system.hpp>
#include <transform.hpp>
#include <utility>
using namespace kuki;
namespace my_game {
SpinSystem::SpinSystem(Application &app)
  : System(std::in_place_type<SpinSystem>, app) {}
auto SpinSystem::Update(const float) -> void {
  // A binding is read in `Update`; a button that a tick must see, through `RegisterButton` and
  // `GetButtons` instead.
  if (app.IsBindingPressed(REVERSE))
    direction = -direction;
}
auto SpinSystem::Tick() -> void {
  const auto step = app.TickTime() * direction;
  MyGameWorld::ForEach<Spin, Transform>(app, [&](const EntityID id, Spin *spin, Transform *transform) {
    transform->rotation = glm::angleAxis(spin->speed * step, glm::vec3(.0f, 1.f, .0f)) * transform->rotation;
    // Without this the change is never applied: `Transform` has no dirty bit of its own.
    app.MarkTransformDirty(id);
  });
}
} // namespace my_game
