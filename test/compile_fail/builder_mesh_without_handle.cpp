// expect: this entity's archetype has no MeshHandle
// Pointing an entity with nowhere to hold a mesh at one.
#include <game_builder.hpp>
auto Build(kuki::Application &app) -> void {
  kuki::Game(app).Scene("Main").Entity<kuki::LightObject>("Sun").Mesh("Cube");
}
