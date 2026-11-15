// expect: this entity's archetype does not have that component
// Filling in a component the entity was not made with, which used to add it.
#include <camera.hpp>
#include <game_builder.hpp>
#include <light.hpp>
auto Build(kuki::Application &app) -> void {
  kuki::Game(app).Scene("Main").Entity<kuki::LightObject>("Sun").With<kuki::Camera>([](kuki::Camera &) {});
}
