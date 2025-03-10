#include <scene_manager.hpp>
SceneManager::~SceneManager() {
  for (const auto& [_, scene] : idToScene)
    delete scene;
  idToScene.clear();
}
unsigned int SceneManager::Create(const std::string& name) {
  auto id = nextID++;
  auto scene = new Scene(name, id);
  idToScene[id] = scene;
  nameToID[name] = id;
  return id;
}
void SceneManager::Delete(unsigned int id) {
  auto it = idToScene.find(id);
  if (it == idToScene.end())
    return;
  nameToID.erase(it->second->GetName());
  delete it->second;
  idToScene.erase(id);
}
void SceneManager::Delete(const std::string& name) {
  auto it = nameToID.find(name);
  if (it == nameToID.end())
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
  auto it = nameToID.find(name);
  if (it == nameToID.end())
    return nullptr;
  return Get(it->second);
}
bool SceneManager::Has(unsigned int id) {
  return idToScene.find(id) != idToScene.end();
}
bool SceneManager::Has(const std::string& name) {
  return nameToID.find(name) != nameToID.end();
}
