#pragma once
#include <asset_manager.hpp>
#include <id.hpp>
#include <material_handle.hpp>
#include <mesh_handle.hpp>
#include <primitive_type.hpp>
#include <skybox_handle.hpp>
namespace kuki {
class ResourceManager {
public:
  ResourceManager(AssetManager &);
  virtual ~ResourceManager() = default;
  virtual auto Clear() -> void = 0;
  template <typename T>
  auto GetResourceID(const T &) -> EntityID;
protected:
};
template <typename T>
auto ResourceManager::GetResourceID(const T &) -> EntityID {
  return EntityID::Invalid;
}
template <>
auto ResourceManager::GetResourceID<MaterialHandle>(const MaterialHandle &) -> EntityID {
  return EntityID::Invalid;
}
template <>
auto ResourceManager::GetResourceID<MeshHandle>(const MeshHandle &) -> EntityID {
  return EntityID::Invalid;
}
template <>
auto ResourceManager::GetResourceID<SkyboxHandle>(const SkyboxHandle &) -> EntityID {
  return EntityID::Invalid;
}
} // namespace kuki
