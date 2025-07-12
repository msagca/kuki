#include <application.hpp>
#include <component/script.hpp>
#include <system/scripting.hpp>
#include <system/system.hpp>
#include <spdlog/spdlog.h>
namespace kuki {
ScriptingSystem::ScriptingSystem(Application& app)
  : System(app) {}
ScriptingSystem::~ScriptingSystem() {
  Shutdown();
}
void ScriptingSystem::Update(float deltaTime) {
  app.ForEachEntity<Script>([this, deltaTime](unsigned int id, Script* _) {
    auto it = scripts.find(id);
    if (it == scripts.end())
      return;
    auto& instance = it->second;
    instance->Update(deltaTime);
  });
}
void ScriptingSystem::Shutdown() {
  for (const auto& [id, script] : scripts)
    delete script;
  scripts.clear();
}
void ScriptingSystem::Unregister(unsigned int id) {
  auto it = scripts.find(id);
  if (it == scripts.end())
    return;
  delete it->second;
  scripts.erase(id);
  spdlog::info("Unregistered script for entity #{}.", id);
}
IScript* ScriptingSystem::GetScript(unsigned int id) {
  auto it = scripts.find(id);
  if (it == scripts.end())
    return nullptr;
  return it->second;
}
} // namespace kuki
