#include <gl_resource_registry.hpp>
#include <string>
#include <utility>
namespace kuki {
auto GLResourceRegistry::Clear() -> void {
  nextId = EntityID{0};
  ids.clear();
  nameToId.clear();
  buffers.Clear();
  computeShaders.Clear();
  litShaders.Clear();
  materials.Clear();
  meshes.Clear();
  renderTargets.Clear();
  skyboxes.Clear();
  textures.Clear();
  unlitShaders.Clear();
}
auto GLResourceRegistry::Create(std::string name) -> EntityID {
  const auto id = nextId++;
  ids.insert(id);
  if (!name.empty())
    nameToId[std::move(name)] = id;
  return id;
}
auto GLResourceRegistry::GetID(const std::string &name) const -> EntityID {
  if (auto it = nameToId.find(name); it != nameToId.end())
    return it->second;
  return EntityID::Invalid;
}
auto GLResourceRegistry::IsEntity(const EntityID id) const -> bool {
  return ids.contains(id);
}
} // namespace kuki
