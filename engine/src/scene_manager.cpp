#include <concepts.hpp>
#include <id.hpp>
#include <scene.hpp>
#include <scene_manager.hpp>
#include <string>
namespace kuki {
auto SceneManager::Activate(const std::string &name) -> bool {
  if (auto it = nameToId.find(name); it != nameToId.end()) {
    const auto id = it->second;
    if (activeScene == id)
      return true;
    Load(id);
    if (auto it2 = idToStatus.find(id); it2 != idToStatus.end())
      if (it2->second == SceneStatus::Loaded)
        if (auto it3 = idToScene.find(id); it3 != idToScene.end()) {
          if (activeScene)
            Deactivate(activeScene);
          it2->second = SceneStatus::Active;
          activeScene = id;
          spdlog::info("[SceneManager] activated scene: {}", idToName[activeScene]);
          return true;
        }
  }
  return false;
}
auto SceneManager::Create(std::string name) -> SceneID {
  if (auto it = nameToId.find(name); it != nameToId.end())
    return it->second;
  const auto id = nextId++;
  idToScene[id] = std::make_unique<Scene>(id);
  nameToId[name] = id;
  idToName[id] = std::move(name);
  idToStatus[id] = SceneStatus::Registered;
  if (!activeScene)
    activeScene = id;
  spdlog::info("[SceneManager] created scene: {}", idToName[id]);
  return id;
}
auto SceneManager::Delete(const std::string &name) -> bool {
  if (auto it = nameToId.find(name); it != nameToId.end()) {
    const auto id = it->second;
    Unload(id);
    if (auto it2 = idToScene.find(id); it2 != idToScene.end()) {
      idToScene.erase(it2);
      idToName.erase(id);
      idToStatus.erase(id);
      return true;
    }
  }
  return false;
}
auto SceneManager::GetName(const SceneID id) const -> std::string {
  if (auto it = idToName.find(id); it != idToName.end())
    return it->second;
  return "";
}
auto SceneManager::GetStatus(const std::string &name) const -> SceneStatus {
  if (auto it = nameToId.find(name); it != nameToId.end()) {
    const auto id = it->second;
    if (auto it2 = idToStatus.find(id); it2 != idToStatus.end())
      return it2->second;
  }
  return SceneStatus::Unregistered;
}
auto SceneManager::IsScene(const std::string &name) const -> bool {
  return nameToId.find(name) != nameToId.end();
}
auto SceneManager::Load(const std::string &name) -> bool {
  if (auto it = nameToId.find(name); it != nameToId.end())
    return Load(it->second);
  return false;
}
auto SceneManager::Rename(const std::string &nameOld, std::string nameNew) -> bool {
  if (auto it = nameToId.find(nameOld); it != nameToId.end()) {
    const auto id = it->second;
    if (nameToId.contains(nameNew))
      return false;
    nameToId.erase(it);
    nameToId[nameNew] = id;
    idToName[id] = std::move(nameNew);
    spdlog::info("[SceneManager] renamed scene: {} -> {}", nameOld, idToName[id]);
    return true;
  }
  return false;
}
auto SceneManager::Unload(const std::string &name) -> bool {
  if (auto it = nameToId.find(name); it != nameToId.end())
    return Unload(it->second);
  return false;
}
auto SceneManager::Deactivate(const SceneID id) -> bool {
  if (auto it = idToStatus.find(id); it != idToStatus.end())
    if (it->second == SceneStatus::Active)
      if (auto it2 = idToScene.find(id); it2 != idToScene.end()) {
        it->second = SceneStatus::Loaded;
        activeScene = SceneID::Invalid;
        if (auto it3 = idToName.find(id); it3 != idToName.end())
          spdlog::info("[SceneManager] deactivated scene: {}", it3->second);
        return true;
      }
  return false;
}
auto SceneManager::Load(const SceneID id) -> bool {
  if (auto it = idToStatus.find(id); it != idToStatus.end())
    if (it->second == SceneStatus::Registered)
      if (auto it2 = idToScene.find(id); it2 != idToScene.end()) {
        it->second = SceneStatus::Loaded;
        if (auto it3 = idToName.find(id); it3 != idToName.end())
          spdlog::info("[SceneManaged] loaded scene: {}", it3->second);
        OnSceneLoaded.Emit(*it2->second);
        return true;
      }
  return false;
}
auto SceneManager::Unload(const SceneID id) -> bool {
  Deactivate(id);
  if (auto it = idToStatus.find(id); it != idToStatus.end())
    if (it->second == SceneStatus::Loaded)
      if (auto it2 = idToScene.find(id); it2 != idToScene.end()) {
        it->second = SceneStatus::Registered;
        if (auto it3 = idToName.find(id); it3 != idToName.end())
          spdlog::info("[SceneManaged] unloaded scene: {}", it3->second);
        return true;
      }
  return false;
}
} // namespace kuki
