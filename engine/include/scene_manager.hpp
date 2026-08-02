#pragma once
#include <event.hpp>
#include <id.hpp>
#include <kuki_engine_export.h>
#include <manager.hpp>
#include <scene.hpp>
#include <string>
#include <unordered_map>
namespace kuki {
class KUKI_ENGINE_API SceneManager final : public Manager {
public:
  SceneManager(Application &);
  auto Create(std::string) -> SceneID;
  auto Get(this auto &self, const std::string & = "") -> ConstCorrectPointer<decltype(self), Scene>;
  auto Switch(const std::string &) -> bool;
private:
  SceneID activeScene{};
  SceneID nextId{SceneID::First};
  std::unordered_map<SceneID, std::string> idToName;
  std::unordered_map<std::string, SceneID> nameToId;
  std::unordered_map<SceneID, std::unique_ptr<Scene>> idToScene;
};
auto SceneManager::Get(this auto &self, const std::string &name) -> ConstCorrectPointer<decltype(self), Scene> {
  if (name.empty()) {
    if (auto it = self.idToScene.find(self.activeScene); it != self.idToScene.end())
      return it->second.get();
  } else if (auto it = self.nameToId.find(name); it != self.nameToId.end()) {
    if (auto it2 = self.idToScene.find(it->second); it2 != self.idToScene.end())
      return it2->second.get();
  }
  return nullptr;
}
} // namespace kuki
