// expect: this entity's archetype is not drawn and has nowhere to hold a mesh
// Pointing an entity with nowhere to hold a mesh at one.
#include <game_builder.hpp>
auto Build(kuki::Application &app) -> void {
  kuki::Game(app).Scene("Main").Entity<kuki::LightObject>("Sun").Mesh("Cube");
}
