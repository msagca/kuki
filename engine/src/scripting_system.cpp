#include <application.hpp>
#include <script.hpp>
#include <scripting_system.hpp>
#include <system.hpp>
#include <utility>
namespace kuki {
ScriptingSystem::ScriptingSystem(Application &app)
  : System(std::in_place_type<ScriptingSystem>, app) {}
auto ScriptingSystem::StartIfIdle(Script *script) -> void {
  if (script->state != ScriptState::Idle)
    return;
  script->Start(app);
  script->state = ScriptState::Started;
}
auto ScriptingSystem::Start() -> void {
  app.ForEachEntity<Script>([this](const EntityID id, Script *script) {
    script->entityId = id;
    StartIfIdle(script);
  });
}
auto ScriptingSystem::Update(const float deltaTime) -> void {
  app.ForEachEntity<Script>([this](const EntityID id, Script *script) {
    script->entityId = id;
    StartIfIdle(script);
    script->Update(app);
  });
}
auto ScriptingSystem::Shutdown() -> void {
  app.ForEachEntity<Script>([this](const EntityID, Script *script) {
    script->Shutdown(app);
  });
}
} // namespace kuki
