// expect: no archetype of this world has every component this query requires
// A query no archetype could answer, which at run time would quietly visit nothing.
#include <camera.hpp>
#include <particle_emitter.hpp>
#include <world.hpp>
auto Visit(kuki::Application &app) -> void {
  kuki::EngineWorld::ForEach<kuki::Camera, kuki::ParticleEmitter>(app, [](kuki::EntityID, kuki::Camera *, kuki::ParticleEmitter *) {});
}
