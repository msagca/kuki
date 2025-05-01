#pragma once
#include "system.hpp"
#include <component/script.hpp>
#include <spdlog/spdlog.h>
namespace kuki {
class Application;
class KUKI_API ScriptingSystem final : public System {
private:
  std::unordered_map<unsigned int, IScript*> scripts;
public:
  ScriptingSystem(Application&);
  ~ScriptingSystem();
  void Update(float) override;
  template <typename T>
  requires std::is_base_of_v<IScript, T>
  void Register(unsigned int);
  void Unregister(unsigned int);
  IScript* GetScript(unsigned int);
};
template <typename T>
requires std::is_base_of_v<IScript, T>
void ScriptingSystem::Register(unsigned int entityId) {
  auto it = scripts.find(entityId);
  if (it != scripts.end()) {
    delete it->second;
    scripts.erase(entityId);
  }
  scripts.emplace(entityId, new T(app, entityId)); // NOTE: using entity ID as key
  spdlog::info("Registered script for entity #{}.", entityId);
}
} // namespace kuki
