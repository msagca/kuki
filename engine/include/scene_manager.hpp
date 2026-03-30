#pragma once
#include <event.hpp>
#include <id.hpp>
#include <kuki_engine_export.h>
#include <scene.hpp>
#include <string>
#include <unordered_map>
namespace kuki {
enum class SceneStatus : uint8_t {
  Active,
  Loaded,
  Registered,
  Unregistered
};
class KUKI_ENGINE_API SceneManager {
public:
  Event<Scene &> OnSceneLoaded;
  auto Activate(const std::string &) -> bool;
  auto Create(std::string) -> SceneID;
  auto Delete(const std::string &) -> bool;
  auto GetName(const SceneID) const -> std::string;
  auto GetStatus(const std::string &) const -> SceneStatus;
  auto IsScene(const std::string &) const -> bool;
  auto Load(const std::string &) -> bool;
  // TODO: implement LoadAsync
  auto Rename(const std::string &, std::string) -> bool;
  auto Unload(const std::string &) -> bool;
  auto Get(this auto &self, const std::string & = "") -> ConstCorrectPointer<decltype(self), Scene>;
  auto GetActive(this auto &self) -> ConstCorrectPointer<decltype(self), Scene>;
private:
  SceneID activeScene{};
  SceneID nextId{SceneID::First};
  std::unordered_map<SceneID, SceneStatus> idToStatus;
  std::unordered_map<SceneID, std::string> idToName;
  std::unordered_map<std::string, SceneID> nameToId;
  std::unordered_map<SceneID, std::unique_ptr<Scene>> idToScene;
  auto Deactivate(const SceneID) -> bool;
  auto Load(const SceneID) -> bool;
  auto Unload(const SceneID) -> bool;
};
auto SceneManager::Get(this auto &self, const std::string &name) -> ConstCorrectPointer<decltype(self), Scene> {
  if (name.empty())
    return self.GetActive();
  if (auto it = self.nameToId.find(name); it != self.nameToId.end())
    if (auto it2 = self.idToScene.find(it->second); it2 != self.idToScene.end())
      return it2->second.get();
  return nullptr;
}
auto SceneManager::GetActive(this auto &self) -> ConstCorrectPointer<decltype(self), Scene> {
  if (auto it = self.idToScene.find(self.activeScene); it != self.idToScene.end())
    return it->second.get();
  return nullptr;
}
} // namespace kuki
