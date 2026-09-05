#pragma once
#include <concepts.hpp>
#include <gl_buffer.hpp>
#include <gl_compute_shader.hpp>
#include <gl_lit_shader.hpp>
#include <gl_material.hpp>
#include <gl_mesh.hpp>
#include <gl_render_target.hpp>
#include <gl_skybox.hpp>
#include <gl_texture.hpp>
#include <gl_unlit_shader.hpp>
#include <id.hpp>
#include <kuki_engine_export.h>
#include <string>
#include <type_traits>
#include <utility>
#include <unordered_map>
#include <unordered_set>
#include <vector>
namespace kuki {
template <typename T>
class ResourcePool {
public:
  auto Add(const EntityID) -> T *;
  auto Clear() -> void;
  auto Get(this auto &, const EntityID) -> decltype(auto);
  auto GetAny(this auto &) -> decltype(auto);
  auto Has(const EntityID) const -> bool;
  auto Remove(const EntityID) -> bool;
  auto ForEach(auto &&) -> void;
private:
  std::unordered_map<EntityID, size_t> idToSlot;
  std::vector<EntityID> slotToId;
  std::vector<T> resources;
};
template <typename T>
auto ResourcePool<T>::Add(const EntityID id) -> T * {
  if (!id)
    return nullptr;
  if (auto it = idToSlot.find(id); it != idToSlot.end())
    return &resources[it->second];
  const auto slot = resources.size();
  resources.emplace_back();
  slotToId.push_back(id);
  idToSlot.emplace(id, slot);
  return &resources[slot];
}
template <typename T>
auto ResourcePool<T>::Clear() -> void {
  idToSlot.clear();
  slotToId.clear();
  resources.clear();
}
template <typename T>
auto ResourcePool<T>::Get(this auto &self, const EntityID id) -> decltype(auto) {
  if (auto it = self.idToSlot.find(id); it != self.idToSlot.end())
    return &self.resources[it->second];
  return ConstCorrectPointer<decltype(self), T>(nullptr);
}
template <typename T>
auto ResourcePool<T>::GetAny(this auto &self) -> decltype(auto) {
  if (!self.resources.empty())
    return &self.resources.front();
  return ConstCorrectPointer<decltype(self), T>(nullptr);
}
template <typename T>
auto ResourcePool<T>::ForEach(auto &&func) -> void {
  for (auto &resource : resources)
    func(resource);
}
template <typename T>
auto ResourcePool<T>::Has(const EntityID id) const -> bool {
  return idToSlot.contains(id);
}
template <typename T>
auto ResourcePool<T>::Remove(const EntityID id) -> bool {
  auto it = idToSlot.find(id);
  if (it == idToSlot.end())
    return false;
  const auto index = it->second;
  const auto last = resources.size() - 1;
  if (index != last) {
    std::swap(resources[index], resources[last]);
    const auto otherId = slotToId[last];
    std::swap(slotToId[index], slotToId[last]);
    idToSlot[otherId] = index;
  }
  idToSlot.erase(id);
  slotToId.pop_back();
  resources.pop_back();
  return true;
}
class KUKI_ENGINE_API GLResourceRegistry {
public:
  auto Clear() -> void;
  auto Create(std::string = "") -> EntityID;
  auto GetID(const std::string &) const -> EntityID;
  auto IsEntity(const EntityID) const -> bool;
  template <typename T>
  auto AddComponent(const EntityID) -> T *;
  template <typename T>
  auto GetAny(this auto &) -> decltype(auto);
  template <typename T>
  auto GetComponent(this auto &, const EntityID) -> decltype(auto);
  template <typename T>
  auto GetComponent(this auto &, const std::string &) -> decltype(auto);
  /// @brief Visits every resource of one kind, so a caller can hand them back to the driver.
  ///
  /// `Clear` cannot do that itself. The registry holds plain integer names and has no idea which
  /// call frees which -- a texture and a framebuffer are both a `GLuint` -- so releasing them is
  /// the renderer's business and this is what lets it reach them.
  template <typename T>
  auto ForEach(auto &&) -> void;
private:
  EntityID nextId{0};
  std::unordered_set<EntityID> ids;
  std::unordered_map<std::string, EntityID> nameToId;
  ResourcePool<GLBuffer> buffers;
  ResourcePool<GLComputeShader> computeShaders;
  ResourcePool<GLLitShader> litShaders;
  ResourcePool<GLMaterial> materials;
  ResourcePool<GLMesh> meshes;
  ResourcePool<GLRenderTarget> renderTargets;
  ResourcePool<GLSkybox> skyboxes;
  ResourcePool<GLTexture> textures;
  ResourcePool<GLUnlitShader> unlitShaders;
  template <typename T>
  auto GetPool(this auto &) -> decltype(auto);
};
template <typename T>
auto GLResourceRegistry::GetPool(this auto &self) -> decltype(auto) {
  if constexpr (std::is_same_v<T, GLBuffer>)
    return (self.buffers);
  else if constexpr (std::is_same_v<T, GLComputeShader>)
    return (self.computeShaders);
  else if constexpr (std::is_same_v<T, GLLitShader>)
    return (self.litShaders);
  else if constexpr (std::is_same_v<T, GLMaterial>)
    return (self.materials);
  else if constexpr (std::is_same_v<T, GLMesh>)
    return (self.meshes);
  else if constexpr (std::is_same_v<T, GLRenderTarget>)
    return (self.renderTargets);
  else if constexpr (std::is_same_v<T, GLSkybox>)
    return (self.skyboxes);
  else if constexpr (std::is_same_v<T, GLTexture>)
    return (self.textures);
  else if constexpr (std::is_same_v<T, GLUnlitShader>)
    return (self.unlitShaders);
  else
    static_assert(!sizeof(T), "GLResourceRegistry: unsupported GL resource type");
}
template <typename T>
auto GLResourceRegistry::ForEach(auto &&func) -> void {
  GetPool<T>().ForEach(std::forward<decltype(func)>(func));
}
template <typename T>
auto GLResourceRegistry::AddComponent(const EntityID id) -> T * {
  if (!ids.contains(id))
    return nullptr;
  return GetPool<T>().Add(id);
}
template <typename T>
auto GLResourceRegistry::GetAny(this auto &self) -> decltype(auto) {
  return self.template GetPool<T>().GetAny();
}
template <typename T>
auto GLResourceRegistry::GetComponent(this auto &self, const EntityID id) -> decltype(auto) {
  return self.template GetPool<T>().Get(id);
}
template <typename T>
auto GLResourceRegistry::GetComponent(this auto &self, const std::string &name) -> decltype(auto) {
  return self.template GetComponent<T>(self.GetID(name));
}
} // namespace kuki
