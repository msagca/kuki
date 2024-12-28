#include <glm/common.hpp>
#include <asset_manager.hpp>
#include <chrono>
#include <component_types.hpp>
#include <glad/glad.h>
#include <glm/ext/vector_float3.hpp>
#include <primitive.hpp>
#include <string>
unsigned int AssetManager::Create(std::string name) {
  if (name.size() > 0) {
    auto assetName = name;
    if (nameToID.find(name) != nameToID.end())
      assetName = GenerateName(name);
    idToName[nextID] = assetName;
  } else
    idToName[nextID] = GenerateName("Asset#");
  nameToID[idToName[nextID]] = nextID;
  idToMask[nextID] = 0;
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
  idToMask.erase(id);
  nameToID.erase(idToName[id]);
  idToName.erase(id);
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
void AssetManager::RemoveAllComponents(unsigned int id) {
  if (idToMask.find(id) == idToMask.end())
    return;
  transformManager.Remove(id);
  meshManager.Remove(id);
  textureManager.Remove(id);
  idToMask[id] = 0;
}
void AssetManager::CleanUp() {
  meshManager.CleanUp();
  textureManager.CleanUp();
  shaderManager.CleanUp();
}
