#pragma once
#include <event.hpp>
#include <id.hpp>
#include <kuki_engine_export.h>
#include <scene.hpp>
#include <string>
#include <unordered_map>
#include <unordered_set>
namespace kuki {
enum class SceneStatus : uint8_t {
  Active,
  Loaded,
  Registered,
  Unregistered
};
class KUKI_ENGINE_API SceneManager {
public:
  Event<const Scene &> OnSceneActivated;
  Event<const Scene &> OnSceneDeactivated;
  Event<const Scene &> OnSceneLoaded;
  Event<const Scene &> OnSceneUnloaded;
  auto Activate(const SceneID) -> bool;
  auto Create(std::string = "") -> SceneID;
  auto Deactivate(const SceneID) -> bool;
  auto Delete(const SceneID) -> bool;
  auto GetName(const SceneID) const -> std::string;
  auto GetStatus(const SceneID) const -> SceneStatus;
  auto Has(const SceneID) const -> bool;
  auto Load(const SceneID) -> bool;
  auto Rename(const SceneID, std::string) -> bool;
  auto Unload(const SceneID) -> bool;
  // templates
  auto Get(this auto &self, const std::string &) -> ConstCorrectPointer<decltype(self), Scene>;
  auto GetActive(this auto &self) -> ConstCorrectPointer<decltype(self), Scene>;
private:
  SceneID activeScene{};
  SceneID nextId{SceneID::First};
  std::unordered_map<SceneID, SceneStatus> idToStatus;
  std::unordered_map<SceneID, std::string> idToName;
  std::unordered_multimap<std::string, SceneID> nameToId;
  std::unordered_map<SceneID, std::unique_ptr<Scene>> idToScene;
};
auto SceneManager::Get(this auto &self, const std::string &name) -> ConstCorrectPointer<decltype(self), Scene> {
  return nullptr;
}
auto SceneManager::GetActive(this auto &self) -> ConstCorrectPointer<decltype(self), Scene> {
  if (auto it = self.idToScene.find(self.activeScene); it != self.idToScene.end())
    return it->second.get();
  return nullptr;
}
} // namespace kuki
