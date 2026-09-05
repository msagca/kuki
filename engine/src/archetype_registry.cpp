#include <animator.hpp>
#include <archetype_registry.hpp>
#include <bone_data.hpp>
#include <bounding_box.hpp>
#include <camera.hpp>
#include <gl_buffer.hpp>
#include <gl_compute_shader.hpp>
#include <gl_lit_shader.hpp>
#include <gl_material.hpp>
#include <gl_mesh.hpp>
#include <gl_render_target.hpp>
#include <gl_skybox.hpp>
#include <gl_texture.hpp>
#include <gl_unlit_shader.hpp>
#include <indirect_lighting.hpp>
#include <light.hpp>
#include <material_handle.hpp>
#include <mesh_handle.hpp>
#include <skeleton.hpp>
#include <skybox_handle.hpp>
#include <texture_handle.hpp>
#include <transform.hpp>
namespace kuki {
auto ArchetypeRegistry::Clear() -> void {
  archetypes.clear();
}
auto ArchetypeRegistry::GetOrCreateArchetype(const ComponentMask &mask) -> Archetype * {
  if (auto it = archetypes.find(mask); it != archetypes.end())
    return it->second.get();
  auto archetype = std::make_unique<Archetype>();
  archetype->signature = mask;
  Component::ForEachSetType(mask, [&](const ComponentType type) {
    if (type == ComponentType::Script)
      return;
    archetype->columnIndexByType[static_cast<size_t>(type)] = static_cast<int>(archetype->columns.size());
    archetype->columns.push_back(CreateColumn(type));
  });
  auto [it, _] = archetypes.emplace(mask, std::move(archetype));
  return it->second.get();
}
auto ArchetypeRegistry::RemoveEntity(const EntityID id, const EntityLocation &from) -> EntityID {
  auto *archetype = GetOrCreateArchetype(from.signature);
  const auto row = from.row;
  const auto last = archetype->entities.size() - 1;
  EntityID displaced{};
  if (row != last)
    displaced = archetype->entities[last];
  for (auto &column : archetype->columns)
    column->SwapPop(row);
  if (row != last)
    archetype->entities[row] = displaced;
  archetype->entities.pop_back();
  return displaced;
}
auto ArchetypeRegistry::MoveEntity(const EntityID id, const EntityLocation &from, const ComponentMask &newMask) -> MoveResult {
  auto *oldArchetype = GetOrCreateArchetype(from.signature);
  auto *newArchetype = GetOrCreateArchetype(newMask);
  const auto oldRow = from.row;
  const auto keptMask = from.signature & newMask;
  const auto addedMask = newMask & ~from.signature;
  Component::ForEachSetType(keptMask, [&](const ComponentType type) {
    if (type == ComponentType::Script)
      return;
    const auto bit = static_cast<size_t>(type);
    auto &oldColumn = *oldArchetype->columns[static_cast<size_t>(oldArchetype->columnIndexByType[bit])];
    auto &newColumn = *newArchetype->columns[static_cast<size_t>(newArchetype->columnIndexByType[bit])];
    newColumn.MoveAppend(oldColumn, oldRow);
  });
  Component::ForEachSetType(addedMask, [&](const ComponentType type) {
    if (type == ComponentType::Script)
      return;
    const auto bit = static_cast<size_t>(type);
    newArchetype->columns[static_cast<size_t>(newArchetype->columnIndexByType[bit])]->EmplaceDefault();
  });
  newArchetype->entities.push_back(id);
  MoveResult result{.location = {newMask, newArchetype->entities.size() - 1}};
  const auto oldLast = oldArchetype->entities.size() - 1;
  if (oldRow != oldLast) {
    result.displacedEntity = oldArchetype->entities[oldLast];
    result.displacedRow = oldRow;
  }
  for (auto &column : oldArchetype->columns)
    column->SwapPop(oldRow);
  if (oldRow != oldLast)
    oldArchetype->entities[oldRow] = result.displacedEntity;
  oldArchetype->entities.pop_back();
  return result;
}
auto ArchetypeRegistry::CreateColumn(const ComponentType type) -> std::unique_ptr<IArchetypeColumn> {
  switch (type) {
  case ComponentType::Animator:
    return std::make_unique<ArchetypeColumn<Animator>>();
  case ComponentType::BoneData:
    return std::make_unique<ArchetypeColumn<BoneData>>();
  case ComponentType::BoundingBox:
    return std::make_unique<ArchetypeColumn<BoundingBox>>();
  case ComponentType::Camera:
    return std::make_unique<ArchetypeColumn<Camera>>();
  case ComponentType::DXMaterial:
    return std::make_unique<ArchetypeColumn<DXMaterial>>();
  case ComponentType::GLBuffer:
    return std::make_unique<ArchetypeColumn<GLBuffer>>();
  case ComponentType::GLComputeShader:
    return std::make_unique<ArchetypeColumn<GLComputeShader>>();
  case ComponentType::GLLitShader:
    return std::make_unique<ArchetypeColumn<GLLitShader>>();
  case ComponentType::GLMaterial:
    return std::make_unique<ArchetypeColumn<GLMaterial>>();
  case ComponentType::GLMesh:
    return std::make_unique<ArchetypeColumn<GLMesh>>();
  case ComponentType::GLRenderTarget:
    return std::make_unique<ArchetypeColumn<GLRenderTarget>>();
  case ComponentType::GLSkybox:
    return std::make_unique<ArchetypeColumn<GLSkybox>>();
  case ComponentType::GLTexture:
    return std::make_unique<ArchetypeColumn<GLTexture>>();
  case ComponentType::GLUnlitShader:
    return std::make_unique<ArchetypeColumn<GLUnlitShader>>();
  case ComponentType::IndirectLighting:
    return std::make_unique<ArchetypeColumn<IndirectLighting>>();
  case ComponentType::Light:
    return std::make_unique<ArchetypeColumn<Light>>();
  case ComponentType::MaterialHandle:
    return std::make_unique<ArchetypeColumn<MaterialHandle>>();
  case ComponentType::MeshHandle:
    return std::make_unique<ArchetypeColumn<MeshHandle>>();
  case ComponentType::ModelMaterialHandle:
    return std::make_unique<ArchetypeColumn<ModelMaterialHandle>>();
  case ComponentType::ModelMeshHandle:
    return std::make_unique<ArchetypeColumn<ModelMeshHandle>>();
  case ComponentType::Script:
    return nullptr;
  case ComponentType::Skeleton:
    return std::make_unique<ArchetypeColumn<Skeleton>>();
  case ComponentType::SkyboxHandle:
    return std::make_unique<ArchetypeColumn<SkyboxHandle>>();
  case ComponentType::TextureHandle:
    return std::make_unique<ArchetypeColumn<TextureHandle>>();
  case ComponentType::Transform:
    return std::make_unique<ArchetypeColumn<Transform>>();
  }
  return nullptr;
}
} // namespace kuki
