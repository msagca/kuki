#include <concepts.hpp>
#include <id.hpp>
#include <scene.hpp>
#include <scene_manager.hpp>
#include <string>
namespace kuki {
auto SceneManager::Create(std::string name) -> SceneID {
  const auto id = nextId++;
  idToScene[id] = std::make_unique<Scene>(id);
  idToName[id] = name;
  nameToId.insert({std::move(name), id});
  idToStatus[id] = SceneStatus::Registered;
  if (!activeScene)
    activeScene = id;
  return id;
}
auto SceneManager::Delete(const SceneID id) -> bool {
  Unload(id);
  if (auto it = idToScene.find(id); it != idToScene.end()) {
    idToScene.erase(it);
    idToName.erase(id);
    idToStatus.erase(id);
    return true;
  }
  return false;
}
auto SceneManager::Load(const SceneID id) -> bool {
  if (auto it = idToStatus.find(id); it != idToStatus.end())
    if (it->second == SceneStatus::Registered)
      if (auto it2 = idToScene.find(id); it2 != idToScene.end()) {
        it->second = SceneStatus::Loaded;
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
        OnSceneUnloaded.Emit(*it2->second);
        return true;
      }
  return false;
}
auto SceneManager::Activate(const SceneID id) -> bool {
  Load(id);
  if (auto it = idToStatus.find(id); it != idToStatus.end())
    if (it->second == SceneStatus::Loaded)
      if (auto it2 = idToScene.find(id); it2 != idToScene.end()) {
        if (activeScene)
          Deactivate(activeScene);
        it->second = SceneStatus::Active;
        activeScene = id;
        OnSceneActivated.Emit(*it2->second);
        return true;
      }
  return false;
}
auto SceneManager::Deactivate(const SceneID id) -> bool {
  if (auto it = idToStatus.find(id); it != idToStatus.end())
    if (it->second == SceneStatus::Active)
      if (auto it2 = idToScene.find(id); it2 != idToScene.end()) {
        it->second = SceneStatus::Loaded;
        activeScene = SceneID::Invalid;
        OnSceneDeactivated.Emit(*it2->second);
        return true;
      }
  return false;
}
auto SceneManager::Has(SceneID id) const -> bool {
  return idToScene.find(id) != idToScene.end();
}
auto SceneManager::GetName(const SceneID id) const -> std::string {
  if (auto it = idToName.find(id); it != idToName.end())
    return it->second;
  return "";
}
auto SceneManager::Rename(const SceneID id, std::string nameNew) -> bool {
  if (auto it = idToName.find(id); it != idToName.end()) {
    const auto &nameOld = it->second;
    auto ids = nameToId.equal_range(nameOld);
    for (auto it2 = ids.first; it2 != ids.second;) {
      if (it2->second == id) {
        it2 = nameToId.erase(it2);
        break;
      } else
        ++it2;
    }
    if (nameNew.empty())
      idToName.erase(it);
    else {
      it->second = nameNew;
      nameToId.insert({std::move(nameNew), id});
    }
    return true;
  }
  return false;
}
auto SceneManager::GetStatus(const SceneID id) const -> SceneStatus {
  if (auto it = idToStatus.find(id); it != idToStatus.end())
    return it->second;
  return SceneStatus::Unregistered;
}
} // namespace kuki
