#include <application.hpp>
#include <profiler.hpp>
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
auto ScriptingSystem::Collect() const -> std::vector<std::pair<EntityID, Script *>> {
  std::vector<std::pair<EntityID, Script *>> scripts;
  app.ForEachEntity<Script>([&scripts](const EntityID id, Script *script) {
    if (script)
      scripts.emplace_back(id, script);
  });
  return scripts;
}
auto ScriptingSystem::Start() -> void {
  for (const auto &[id, script] : Collect()) {
    if (!app.IsEntity(id))
      continue;
    script->entityId = id;
    script->app = &app;
    StartIfIdle(script);
  }
}
auto ScriptingSystem::Update(const float deltaTime) -> void {
  KUKI_PROFILE_SCOPE("Scripting");
  for (const auto &[id, script] : Collect()) {
    if (!app.IsEntity(id))
      continue;
    script->entityId = id;
    script->app = &app;
    StartIfIdle(script);
    script->Update(app);
  }
}
auto ScriptingSystem::Shutdown() -> void {
  for (const auto &[id, script] : Collect()) {
    if (!app.IsEntity(id))
      continue;
    script->app = &app;
    script->Shutdown(app);
  }
}
} // namespace kuki
