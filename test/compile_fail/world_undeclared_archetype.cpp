// expect: this world does not declare that archetype
// An entity of an archetype the game never declared, which at run time would make nothing.
#include <light.hpp>
#include <world.hpp>
auto Make(kuki::Application &app) -> kuki::EntityID {
  return kuki::EngineWorld::Create<kuki::Archetype<kuki::Light>>(app, "lamp");
}
