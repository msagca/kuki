#include <asset_database.hpp>
#include <asset_metadata.hpp>
#include <span>
namespace kuki {
AssetID AssetDatabase::Register(const std::filesystem::path &path) {
  if (auto it = pathToId.find(path); it != pathToId.end())
    return it->second;
  AssetMetadata metadata{
    .id = AssetID::Generate(),
    .name = path.filename().stem().string(),
    .path = path,
    .type = DeduceType(path),
    .status = AssetStatus::Registered};
  idToMetadata.emplace(metadata.id, metadata);
  pathToId.emplace(path, metadata.id);
  return metadata.id;
}
bool AssetDatabase::Unregister(const AssetID id) {
  if (auto it = idToMetadata.find(id); it != idToMetadata.end())
    pathToId.erase(it->second.path);
  return idToMetadata.erase(id) > 0;
}
void AssetDatabase::Unregister(std::span<const AssetID> ids) {
  for (const auto &id : ids)
    Unregister(id);
}
void AssetDatabase::Reset() {
  idToMetadata.clear();
  pathToId.clear();
}
std::filesystem::path AssetDatabase::GetPath(const AssetID id) const {
  if (auto it = idToMetadata.find(id); it != idToMetadata.end())
    return it->second.path;
  return std::filesystem::path{};
}
AssetStatus AssetDatabase::GetStatus(const AssetID id) const {
  if (auto it = idToMetadata.find(id); it != idToMetadata.end())
    return it->second.status;
  return AssetStatus::Unregistered;
}
AssetType AssetDatabase::GetType(const AssetID id) const {
  if (auto it = idToMetadata.find(id); it != idToMetadata.end())
    return it->second.type;
  return AssetType::Unknown;
}
std::string AssetDatabase::GetName(const AssetID id) const {
  if (auto it = idToMetadata.find(id); it != idToMetadata.end())
    return it->second.name;
  return {};
}
bool AssetDatabase::SetStatus(const AssetID id, const AssetStatus status) {
  if (status == AssetStatus::Unregistered)
    return false;
  if (auto it = idToMetadata.find(id); it != idToMetadata.end()) {
    it->second.status = status;
    return true;
  }
  return false;
}
AssetType AssetDatabase::DeduceType(const std::filesystem::path &path) {
  static const std::unordered_map<std::string, AssetType> extToType = {
    {".bmp", AssetType::Texture},
    {".comp", AssetType::Shader},
    {".dae", AssetType::Scene},
    {".exr", AssetType::Skybox},
    {".fbx", AssetType::Scene},
    {".frag", AssetType::Shader},
    {".geom", AssetType::Shader},
    {".gif", AssetType::Texture},
    {".glb", AssetType::Scene},
    {".glsl", AssetType::Shader},
    {".gltf", AssetType::Scene},
    {".hdr", AssetType::Skybox},
    {".hlsl", AssetType::Shader},
    {".jpeg", AssetType::Texture},
    {".jpg", AssetType::Texture},
    {".js", AssetType::Script},
    {".lua", AssetType::Script},
    {".mp3", AssetType::Audio},
    {".obj", AssetType::Scene},
    {".ogg", AssetType::Audio},
    {".png", AssetType::Texture},
    {".tga", AssetType::Texture},
    {".vert", AssetType::Shader},
    {".wav", AssetType::Audio}};
  auto ext = path.extension().string();
  if (auto it = extToType.find(ext); it != extToType.end())
    return it->second;
  return AssetType::Unknown;
}
} // namespace kuki
