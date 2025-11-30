#pragma once
#include <entity_manager.hpp>
#include <id.hpp>
#include <kuki_engine_export.h>
#include <octree.hpp>
namespace kuki {
class Camera;
class KUKI_ENGINE_API Scene {
private:
  const std::string name;
  size_t id{0};
public:
  Scene(const std::string&, unsigned int);
  // TODO: expose helper functions to hide EntityManager and Octree details
  EntityManager entityManager{};
  Octree<ID> octree{}; // TODO: move this into EntityManager
  // NOTE: for most scenes, a quadtree would be more appropriate
  std::string GetName() const;
  unsigned int GetId() const;
  Camera* GetCamera();
  ID CreateEntity(std::string&);
  void DeleteEntity(ID);
  void DeleteEntity(const std::string&);
  void DeleteAllEntities();
  void DeleteAllEntities(const std::string&);
  void SortTransforms();
  void UpdateTransforms();
  template <typename F>
  void ForEachVisibleEntity(const Camera&, F&&);
  template <typename F>
  void ForEachOctreeNode(F&&);
  template <typename F>
  void ForEachOctreeLeafNode(F&&);
};
template <typename F>
void Scene::ForEachVisibleEntity(const Camera& camera, F&& func) {
  auto func_ = std::forward(func);
  octree.ForEachInFrustum(camera, [&](ID id) {
    func_(id);
  });
}
template <typename F>
void Scene::ForEachOctreeNode(F&& func) {
  octree.ForEach(func);
}
template <typename F>
void Scene::ForEachOctreeLeafNode(F&& func) {
  octree.ForEachLeaf(func);
}
} // namespace kuki
