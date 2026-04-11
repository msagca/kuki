#include <application.hpp>
#include <id.hpp>
#include <scene_manager.hpp>
#include <script.hpp>
#include <scripting_system.hpp>
#include <system.hpp>
namespace kuki {
ScriptingSystem::ScriptingSystem(Application &app)
  : System(std::in_place_type<ScriptingSystem>), app(app) {}
ScriptingSystem::~ScriptingSystem() {}
auto ScriptingSystem::Start() -> void {
  app.ForEachEntity<Script>([this](const EntityID, Script *script) {
    script->Start(app);
  });
}
auto ScriptingSystem::Update(const float deltaTime) -> void {
  app.ForEachEntity<Script>([&](const EntityID, Script *script) {
    script->Update(app);
  });
}
auto ScriptingSystem::Shutdown() -> void {
  app.ForEachEntity<Script>([this](const EntityID, Script *script) {
    script->Shutdown(app);
  });
}
} // namespace kuki
