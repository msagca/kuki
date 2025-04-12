#include <scene.hpp>
#include <scene_manager.hpp>
#include <string>
SceneManager::~SceneManager() {
  for (const auto& [_, scene] : idToScene)
    delete scene;
  idToScene.clear();
}
unsigned int SceneManager::Create(const std::string& name) {
  auto id = nextId++;
  auto scene = new Scene(name, id);
  idToScene[id] = scene;
  nameToId[name] = id;
  return id;
}
void SceneManager::Delete(unsigned int id) {
  auto it = idToScene.find(id);
  if (it == idToScene.end())
    return;
  nameToId.erase(it->second->GetName());
  delete it->second;
  idToScene.erase(id);
}
void SceneManager::Delete(const std::string& name) {
  auto it = nameToId.find(name);
  if (it == nameToId.end())
    return;
  Delete(it->second);
}
Scene* SceneManager::Get(unsigned int id) {
  auto it = idToScene.find(id);
  if (it == idToScene.end())
    return nullptr;
  return it->second;
}
Scene* SceneManager::Get(const std::string& name) {
  auto it = nameToId.find(name);
  if (it == nameToId.end())
    return nullptr;
  return Get(it->second);
}
bool SceneManager::Has(unsigned int id) {
  return idToScene.find(id) != idToScene.end();
}
bool SceneManager::Has(const std::string& name) {
  return nameToId.find(name) != nameToId.end();
}
