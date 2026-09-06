#include <application.hpp>
#include <concepts.hpp>
#include <id.hpp>
#include <manager.hpp>
#include <memory>
#include <scene.hpp>
#include <scene_manager.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include <utility>
namespace kuki {
SceneManager::SceneManager(Application &app)
  : Manager(app) {}
auto SceneManager::Create(std::string name) -> SceneID {
  if (auto it = nameToId.find(name); it != nameToId.end())
    return it->second;
  const auto id = nextId++;
  idToScene[id] = std::make_unique<Scene>(id);
  nameToId[name] = id;
  idToName[id] = std::move(name);
  if (!activeScene)
    activeScene = id;
  spdlog::info("[SceneManager] Created scene: {}", idToName[id]);
  return id;
}
auto SceneManager::Switch(const std::string &name) -> bool {
  if (auto it = nameToId.find(name); it != nameToId.end()) {
    activeScene = it->second;
    spdlog::info("[SceneManager] Switched to scene: {}", name);
    return true;
  }
  return false;
}
} // namespace kuki
