#pragma once
#include <component/camera.hpp>
#include <component/mesh.hpp>
#include <glm/ext/vector_float3.hpp>
#include <kuki_engine_export.h>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
namespace kuki {
enum class Octant : uint8_t {
  LeftBottomBack,
  LeftBottomFront,
  LeftTopBack,
  LeftTopFront,
  RightBottomBack,
  RightBottomFront,
  RightTopBack,
  RightTopFront,
  None
};
class Octree;
class KUKI_ENGINE_API OctreeNode {
  friend class Octree;
private:
  OctreeNode* children[8]{};
  bool leaf{true};
  const Octant octant;
  const unsigned int maxItems;
  const unsigned int minItems;
  std::unordered_set<unsigned int> items;
  bool Insert(const unsigned int, const BoundingBox&, std::unordered_map<unsigned int, OctreeNode*>&);
  bool Intersects(const BoundingBox&);
  bool Subdivide();
  /// @brief Merge items of children into this node, then remove the children
  void Collapse(std::unordered_map<unsigned int, OctreeNode*>&);
  /// @return true if the total number of items contained within this node is not greater than minItems, false otherwise
  bool CanCollapse() const;
  /// @brief Delete all child nodes
  void Clear(std::unordered_map<unsigned int, OctreeNode*>&);
  /// @brief Get the total number of items inside this node and its children
  unsigned int GetCount() const;
  /// @brief Execute a function on each node in the hierarchy
  template <typename F>
  void ForEach(F);
  template <typename F>
  void ForEachLeaf(F);
  template <typename F>
  void ForEachInFrustum(const Camera&, F);
  void InsertToStream(std::ostringstream&) const;
public:
  OctreeNode(Octant, glm::vec3, glm::vec3, unsigned int, unsigned int, unsigned int, unsigned int);
  ~OctreeNode();
  const BoundingBox bounds;
  const glm::vec3 center;
  const glm::vec3 extent;
  const unsigned int depth;
  const unsigned int maxDepth;
};
class KUKI_ENGINE_API Octree {
private:
  OctreeNode root;
  std::unordered_map<unsigned int, OctreeNode*> itemToNode;
public:
  /// @brief
  /// @param center Center of the octree
  /// @param extent Extent of the octree
  /// @param maxDepth Maximum depth of the octree
  /// @param minItems Minimum number of items in a node before it can be merged
  /// @param maxItems Maximum number of items in a node before it can be subdivided
  Octree(glm::vec3 = glm::vec3{.0f}, glm::vec3 = glm::vec3{10.0f}, unsigned int = 4, unsigned int = 16, unsigned int = 128);
  /// @brief Insert an item into the octree
  /// @param item Key to store the item
  /// @param bounds Bounding box of the item
  /// @return true if the item was inserted, false otherwise
  bool Insert(const unsigned int, const BoundingBox&);
  /// @brief Find the item in octree, and remove it
  /// @return true if the item was found and deleted, false otherwise
  bool Delete(const unsigned int);
  unsigned int GetCount() const;
  std::string ToString() const;
  template <typename F>
  void ForEach(F);
  template <typename F>
  void ForEachLeaf(F);
  template <typename F>
  void ForEachInFrustum(const Camera&, F);
};
template <typename F>
void OctreeNode::ForEach(F func) {
  func(this);
  if (leaf)
    return;
  for (auto i = 0; i < 8; ++i) {
    if (!children[i])
      continue;
    children[i]->ForEach(func);
  }
}
template <typename F>
void OctreeNode::ForEachLeaf(F func) {
  if (leaf) {
    func(this, static_cast<Octant>(octant));
    return;
  }
  for (auto i = 0; i < 8; ++i) {
    if (!children[i])
      continue;
    children[i]->ForEachLeaf(func);
  }
}
template <typename F>
void OctreeNode::ForEachInFrustum(const Camera& camera, F func) {
  if (!camera.IntersectsFrustum(bounds))
    return;
  for (const auto& item : items)
    func(item);
  for (auto i = 0; i < 8; ++i) {
    if (!children[i])
      continue;
    children[i]->ForEachInFrustum(camera, func);
  }
}
template <typename F>
void Octree::ForEach(F func) {
  root.ForEach(func);
}
template <typename F>
void Octree::ForEachLeaf(F func) {
  root.ForEachLeaf(func);
}
template <typename F>
void Octree::ForEachInFrustum(const Camera& camera, F func) {
  root.ForEachInFrustum(camera, func);
}
} // namespace kuki
