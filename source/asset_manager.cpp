#include <asset_manager.hpp>
#include <chrono>
#include <component_types.hpp>
#include <string>
#include <vector>
unsigned int AssetManager::Create(std::string name) {
  if (name.size() > 0) {
    auto& assetName = name;
    if (nameToID.find(name) != nameToID.end())
      assetName = GenerateName(name);
    idToName[nextID] = assetName;
  } else
    idToName[nextID] = GenerateName("Asset#");
  nameToID[idToName[nextID]] = nextID;
  idToMask[nextID] = 0;
  idToChildren.insert({nextID, {}});
  return nextID++;
}
std::string AssetManager::GenerateName(const std::string& name) {
  auto now = std::chrono::high_resolution_clock::now();
  auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
  std::hash<std::string> hashFunc;
  auto hash = hashFunc(name + std::to_string(timestamp));
  return name + std::to_string(hash);
}
void AssetManager::Remove(unsigned int id) {
  RemoveAllComponents(id);
  ForEachChild(id, [&](unsigned int childID, const std::string _) {
    Remove(childID);
  });
  idToMask.erase(id);
  nameToID.erase(idToName[id]);
  idToName.erase(id);
  idToChildren.erase(id);
}
bool AssetManager::Rename(unsigned int id, std::string name) {
  if (idToName.find(id) == idToName.end() || name.size() == 0 || nameToID.find(name) != nameToID.end())
    return false;
  nameToID.erase(idToName[id]);
  nameToID[name] = id;
  idToName[id] = name;
  return true;
}
const std::string& AssetManager::GetName(unsigned int id) {
  static const std::string emptyString = "";
  auto it = idToName.find(id);
  if (it != idToName.end())
    return it->second;
  return emptyString;
}
int AssetManager::GetID(const std::string& name) {
  auto it = nameToID.find(name);
  if (it != nameToID.end())
    return it->second;
  return -1;
}
void AssetManager::AddChild(unsigned int parent, unsigned int child) {
  auto it = idToChildren.find(parent);
  if (it == idToChildren.end())
    return;
  it->second.insert(child);
}
void AssetManager::RemoveChild(unsigned int parent, unsigned int child) {
  auto it = idToChildren.find(parent);
  if (it == idToChildren.end())
    return;
  it->second.erase(child);
}
void AssetManager::RemoveAllComponents(unsigned int id) {
  if (idToMask.find(id) == idToMask.end())
    return;
  transformManager.Remove(id);
  meshManager.Remove(id);
  materialManager.Remove(id);
  shaderManager.Remove(id);
  idToMask[id] = 0;
}
std::vector<IComponent*> AssetManager::GetAllComponents(unsigned int id) {
  std::vector<IComponent*> components;
  if (HasComponent<Transform>(id))
    components.emplace_back(GetComponent<Transform>(id));
  if (HasComponent<Mesh>(id))
    components.emplace_back(GetComponent<Mesh>(id));
  if (HasComponent<Material>(id))
    components.emplace_back(GetComponent<Material>(id));
  return components;
}
void AssetManager::CleanUp() {
  meshManager.CleanUp();
  materialManager.CleanUp();
  shaderManager.CleanUp();
}
