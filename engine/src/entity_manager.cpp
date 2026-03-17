#include <bone_data.hpp>
#include <camera.hpp>
#include <component.hpp>
#include <component_cloner.hpp>
#include <component_manager.hpp>
#include <component_type.hpp>
#include <concepts.hpp>
#include <entity_manager.hpp>
#include <id.hpp>
#include <light.hpp>
#include <list>
#include <material_handle.hpp>
#include <mesh_handle.hpp>
#include <skybox_handle.hpp>
#include <string>
#include <transform.hpp>
#include <unordered_map>
#include <vector>
namespace kuki {
auto EntityManager::AddChild(const EntityID parent, const EntityID child, bool keepWorld) -> bool {
  if (!parent)
    return false;
  if (idToMask.find(parent) == idToMask.end() || idToMask.find(child) == idToMask.end())
    return false;
  if (idToChildren.find(parent) == idToChildren.end())
    idToChildren[parent] = {};
  rootEntities.erase(child);
  auto transformManager = GetManager<Transform>();
  auto childTransform = transformManager->Get(child);
  if (!childTransform)
    childTransform = &transformManager->Add(child);
  auto parentTransform = transformManager->Get(parent);
  if (!parentTransform)
    parentTransform = &transformManager->Add(parent);
  childTransform->parent = parent;
  childTransform->Reparent(parentTransform, keepWorld);
  idToChildren[parent].insert(child);
  idToParent[child] = parent;
  transformManager->Sort(); // TODO: replace this with a partial sort function
  return true;
}
auto EntityManager::AddComponent(const EntityID id, const ComponentType type) -> void {
  switch (type) {
  case ComponentType::BoneData:
    AddComponent<BoneData>(id);
    break;
  case ComponentType::Camera:
    AddComponent<Camera>(id);
    break;
  case ComponentType::GLMaterial:
    AddComponent<GLMaterial>(id);
    break;
  case ComponentType::GLMesh:
    AddComponent<GLMesh>(id);
    break;
  case ComponentType::GLSkybox:
    AddComponent<GLSkybox>(id);
    break;
  case ComponentType::GLTexture:
    AddComponent<GLTexture>(id);
    break;
  case ComponentType::Light:
    AddComponent<Light>(id);
    break;
  case ComponentType::Transform:
    AddComponent<Transform>(id);
    break;
  default:
    break;
  }
}
auto EntityManager::CopyFrom(const EntityManager &other, const EntityID otherId) -> EntityID {
  if (!other.IsEntity(otherId))
    return EntityID::Invalid;
  const auto id = Create(other.GetName(otherId));
  auto cloner = ComponentCloner(*this, id);
  other.ForEachComponent(otherId, [&](const ComponentVariant component) {
    std::visit(cloner, component);
  });
  other.ForEachChild(otherId, [&](const EntityID otherChildId) {
    const auto childId = CopyFrom(other, otherChildId);
    AddChild(id, childId);
  });
  return id;
}
auto EntityManager::CopyTo(const EntityID id, EntityManager &other) const -> EntityID {
  return other.CopyFrom(*this, id);
}
auto EntityManager::Create(std::string name) -> EntityID {
  const auto id = nextId++;
  if (!name.empty()) {
    idToName[id] = name;
    nameToId.insert({std::move(name), id});
  }
  ComponentMask mask{};
  idToMask[id] = mask;
  maskToIdSet[mask].insert(id);
  rootEntities.insert(id);
  return id;
}
auto EntityManager::Delete(const EntityID id) -> bool {
  if (!RemoveAllComponents(id))
    return false;
  ForEachChild(id, [this](const EntityID childId) {
    Delete(childId);
  });
  DeleteRecords(id);
  return true;
}
auto EntityManager::DeleteAll() -> void {
  idToChildren.clear();
  idToMask.clear();
  idToName.clear();
  idToParent.clear();
  maskToIdSet.clear();
  nameToId.clear();
  registeredTypes.clear();
  rootEntities.clear();
  typeIndexToManager.clear();
}
auto EntityManager::GetComponent(const EntityID id, const ComponentType type) -> ComponentVariant {
  switch (type) {
  case ComponentType::BoneData:
    return GetComponent<BoneData>(id);
  case ComponentType::Camera:
    return GetComponent<Camera>(id);
  case ComponentType::GLMaterial:
    return GetComponent<GLMaterial>(id);
  case ComponentType::GLMesh:
    return GetComponent<GLMesh>(id);
  case ComponentType::GLSkybox:
    return GetComponent<GLSkybox>(id);
  case ComponentType::GLTexture:
    return GetComponent<GLTexture>(id);
  case ComponentType::Light:
    return GetComponent<Light>(id);
  case ComponentType::Transform:
    return GetComponent<Transform>(id);
  case ComponentType::MaterialHandle:
    return GetComponent<MaterialHandle>(id);
  case ComponentType::MeshHandle:
    return GetComponent<MeshHandle>(id);
  case ComponentType::SkyboxHandle:
    return GetComponent<SkyboxHandle>(id);
  default: // invalid type
    return std::monostate{};
  }
}
auto EntityManager::GetComponent(const EntityID id, const ComponentType type) const -> const ComponentVariant {
  switch (type) {
  case ComponentType::BoneData:
    return {const_cast<BoneData *>(GetComponent<BoneData>(id))};
  case ComponentType::Camera:
    return {const_cast<Camera *>(GetComponent<Camera>(id))};
  case ComponentType::GLMaterial:
    return {const_cast<GLMaterial *>(GetComponent<GLMaterial>(id))};
  case ComponentType::GLMesh:
    return {const_cast<GLMesh *>(GetComponent<GLMesh>(id))};
  case ComponentType::GLSkybox:
    return {const_cast<GLSkybox *>(GetComponent<GLSkybox>(id))};
  case ComponentType::GLTexture:
    return {const_cast<GLTexture *>(GetComponent<GLTexture>(id))};
  case ComponentType::Light:
    return {const_cast<Light *>(GetComponent<Light>(id))};
  case ComponentType::Transform:
    return {const_cast<Transform *>(GetComponent<Transform>(id))};
  case ComponentType::MaterialHandle:
    return {const_cast<MaterialHandle *>(GetComponent<MaterialHandle>(id))};
  case ComponentType::MeshHandle:
    return {const_cast<MeshHandle *>(GetComponent<MeshHandle>(id))};
  case ComponentType::SkyboxHandle:
    return {const_cast<SkyboxHandle *>(GetComponent<SkyboxHandle>(id))};
  default: // invalid type
    return std::monostate{};
  }
}
auto EntityManager::GetComponentTypes(const EntityID id) const -> std::vector<ComponentType> {
  std::vector<ComponentType> components;
  if (auto it = idToMask.find(id); it != idToMask.end()) {
    const auto &mask = it->second;
    Component::ForEachSetType(mask, [&](const ComponentType type) {
      components.emplace_back(type);
    });
  }
  return components;
}
auto EntityManager::GetCount() const -> size_t {
  return idToMask.size();
}
auto EntityManager::GetMissingComponentTypes(const EntityID id) const -> std::vector<ComponentType> {
  std::vector<ComponentType> components;
  if (auto it = idToMask.find(id); it != idToMask.end()) {
    const auto &mask = it->second;
    Component::ForEachUnsetType(mask, [this, &id, &components](const ComponentType type) {
      components.emplace_back(type);
    });
  }
  return components;
}
auto EntityManager::GetName(const EntityID id) const -> std::string {
  if (auto it = idToName.find(id); it != idToName.end())
    return it->second;
  if (auto it = idToMask.find(id); it != idToMask.end())
    return it->first.ToString();
  return "";
}
auto EntityManager::GetID(const std::string &name) const -> EntityID {
  auto ids = nameToId.equal_range(name);
  if (auto it = ids.first; it != ids.second)
    return it->second;
  return EntityID::Invalid;
}
auto EntityManager::GetParent(const EntityID id) const -> EntityID {
  if (auto it = idToParent.find(id); it != idToParent.end())
    return it->second;
  return EntityID::Invalid;
}
auto EntityManager::HasChildren(const EntityID id) const -> bool {
  if (auto it = idToChildren.find(id); it != idToChildren.end())
    return it->second.size() > 0;
  return false;
}
auto EntityManager::HasParent(const EntityID id) const -> bool {
  return idToParent.find(id) != idToParent.end();
}
auto EntityManager::IsEntity(const EntityID id) const -> bool {
  return idToMask.find(id) != idToMask.end();
}
auto EntityManager::IsEntity(const std::string &name) const -> bool {
  auto ids = nameToId.equal_range(name);
  if (auto it = ids.first; it != ids.second)
    return true;
  return false;
}
auto EntityManager::RemoveChild(const EntityID parent, const EntityID child) -> bool {
  auto it = idToChildren.find(parent);
  if (it == idToChildren.end())
    return false;
  it->second.erase(child);
  if (it->second.empty())
    idToChildren.erase(it->first);
  auto transformManager = GetManager<Transform>();
  auto childTransform = transformManager->Get(child);
  if (childTransform) {
    childTransform->parent = EntityID::Invalid;
    childTransform->Reparent(nullptr);
  }
  idToParent.erase(child);
  rootEntities.insert(child);
  transformManager->Sort();
  return true;
}
auto EntityManager::RemoveComponent(const EntityID id, const ComponentType type) -> bool {
  switch (type) {
  case ComponentType::BoneData:
    return RemoveComponent<BoneData>(id);
  case ComponentType::Camera:
    return RemoveComponent<Camera>(id);
  case ComponentType::GLMaterial:
    return RemoveComponent<GLMaterial>(id);
  case ComponentType::GLMesh:
    return RemoveComponent<GLMesh>(id);
  case ComponentType::GLSkybox:
    return RemoveComponent<GLSkybox>(id);
  case ComponentType::GLTexture:
    return RemoveComponent<GLTexture>(id);
  case ComponentType::Light:
    return RemoveComponent<Light>(id);
  case ComponentType::Transform:
    return RemoveComponent<Transform>(id);
  default:
    return false;
  }
}
auto EntityManager::RemoveAllComponents(const EntityID id) -> bool {
  if (auto it = idToMask.find(id); it != idToMask.end()) {
    Component::ForEachSetType(it->second, [this, &id](const ComponentType type) {
      const auto typeIndex = Component::GetTypeIndex(type);
      if (auto it = typeIndexToManager.find(typeIndex); it != typeIndexToManager.end()) {
        auto &manager = it->second;
        manager->Remove(id);
      }
    });
    if (auto it2 = maskToIdSet.find(it->second); it2 != maskToIdSet.end())
      it2->second.erase(id);
    ComponentMask mask{0};
    maskToIdSet[mask].insert(id);
    idToMask[id] = mask;
    return true;
  }
  return false;
}
auto EntityManager::Rename(const EntityID id, std::string nameNew) -> bool {
  if (auto it = idToName.find(id); it != idToName.end()) {
    const auto &nameOld = it->second;
    auto ids = nameToId.equal_range(nameOld);
    for (auto &it2 = ids.first; it2 != ids.second;) {
      if (it2->second == id) {
        it2 = nameToId.erase(it2);
        break;
      } else
        ++it2;
    }
    if (nameNew.empty())
      idToName.erase(it);
    else {
      it->second = nameNew;
      nameToId.insert({std::move(nameNew), id});
    }
    return true;
  } else if (auto it = idToMask.find(id); it != idToMask.end()) {
    idToName[id] = nameNew;
    nameToId.insert({std::move(nameNew), id});
    return true;
  }
  return false;
}
auto EntityManager::Update() -> void {
  for (auto &[_, manager] : typeIndexToManager)
    manager->Update();
}
auto EntityManager::DeleteRecords(const EntityID id) -> void {
  if (auto it = idToName.find(id); it != idToName.end()) {
    const auto &name = it->second;
    auto ids = nameToId.equal_range(name);
    for (auto &it2 = ids.first; it2 != ids.second;) {
      if (it2->second == id) {
        it2 = nameToId.erase(it2);
        break;
      } else
        ++it2;
    }
  }
  idToName.erase(id);
  idToChildren.erase(id);
  idToParent.erase(id);
  rootEntities.erase(id);
}
} // namespace kuki
