#pragma once
#include <camera.hpp>
#include <format>
#include <glm/ext/vector_float3.hpp>
#include <id.hpp>
#include <kuki_engine_export.h>
#include <mesh.hpp>
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
template <typename T>
class Octree;
template <typename T>
class KUKI_ENGINE_API OctreeNode {
  friend class Octree<T>;
private:
  OctreeNode* children[8]{};
  bool leaf{true};
  const Octant octant;
  const size_t maxItems;
  const size_t minItems;
  std::unordered_set<T> items;
  // TODO: store item bounds for future redistribution operations
  bool Insert(const T, const BoundingBox&, std::unordered_map<T, OctreeNode*>&);
  bool Intersects(const BoundingBox&);
  bool Subdivide();
  /// @brief Merge items of children into this node, then remove the children
  void Collapse(std::unordered_map<T, OctreeNode*>&);
  /// @return true if the total number of items contained within this node is not greater than minItems, false otherwise
  bool CanCollapse() const;
  /// @brief Delete all child nodes
  void Clear(std::unordered_map<T, OctreeNode*>&);
  /// @brief Get the total number of items inside this node and its children
  size_t GetCount() const;
  /// @brief Execute a function on each node in the hierarchy
  template <typename F>
  void ForEach(F);
  template <typename F>
  void ForEachLeaf(F);
  template <typename F>
  void ForEachInFrustum(const Camera&, F);
  void InsertToStream(std::ostringstream&) const;
public:
  OctreeNode(Octant, glm::vec3, glm::vec3, size_t, size_t, size_t, size_t);
  ~OctreeNode();
  const BoundingBox bounds;
  const glm::vec3 center;
  const glm::vec3 extent;
  const size_t depth;
  const size_t maxDepth;
};
template <typename T>
class KUKI_ENGINE_API Octree {
private:
  OctreeNode<T> root;
  std::unordered_map<T, OctreeNode<T>*> itemToNode;
public:
  /// @brief
  /// @param center Center of the octree
  /// @param extent Extent of the octree
  /// @param maxDepth Maximum depth of the octree
  /// @param minItems Minimum number of items in a node before it can be merged
  /// @param maxItems Maximum number of items in a node before it can be subdivided
  Octree(glm::vec3 = glm::vec3{.0f}, glm::vec3 = glm::vec3{10.0f}, size_t = 4, size_t = 16, size_t = 128);
  /// @brief Insert an item into the octree
  /// @param item Key to store the item
  /// @param bounds Bounding box of the item
  /// @return true if the item was inserted, false otherwise
  bool Insert(const T, const BoundingBox&);
  /// @brief Find the item in octree, and remove it
  /// @return true if the item was found and deleted, false otherwise
  bool Delete(const T);
  void Clear();
  size_t GetCount() const;
  std::string ToString() const;
  template <typename F>
  void ForEach(F);
  template <typename F>
  void ForEachLeaf(F);
  template <typename F>
  void ForEachInFrustum(const Camera&, F);
};
template <typename T>
template <typename F>
void OctreeNode<T>::ForEach(F func) {
  func(this);
  if (leaf)
    return;
  for (auto i = 0; i < 8; ++i) {
    if (!children[i])
      continue;
    children[i]->ForEach(func);
  }
}
template <typename T>
template <typename F>
void OctreeNode<T>::ForEachLeaf(F func) {
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
template <typename T>
template <typename F>
void OctreeNode<T>::ForEachInFrustum(const Camera& camera, F func) {
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
template <typename T>
template <typename F>
void Octree<T>::ForEach(F func) {
  root.ForEach(func);
}
template <typename T>
template <typename F>
void Octree<T>::ForEachLeaf(F func) {
  root.ForEachLeaf(func);
}
template <typename T>
template <typename F>
void Octree<T>::ForEachInFrustum(const Camera& camera, F func) {
  root.ForEachInFrustum(camera, func);
}
template <typename T>
OctreeNode<T>::OctreeNode(Octant octant, glm::vec3 center, glm::vec3 extent, size_t depth, size_t maxDepth, size_t minItems, size_t maxItems)
  : octant(octant), center(center), extent(extent), depth(depth), maxDepth(maxDepth), minItems(minItems), maxItems(maxItems), bounds(center - extent, center + extent) {
  if (minItems > maxItems)
    throw std::invalid_argument(std::format("Octree: minItems ({}) cannot be greater than maxItems ({}).", minItems, maxItems));
}
template <typename T>
OctreeNode<T>::~OctreeNode() {
  for (auto i = 0; i < 8; ++i) {
    delete children[i];
    children[i] = nullptr;
  }
}
template <typename T>
Octree<T>::Octree(glm::vec3 center, glm::vec3 extent, size_t maxDepth, size_t minItems, size_t maxItems)
  : root(Octant::None, center, extent, 0, maxDepth, minItems, maxItems) {}
template <typename T>
bool Octree<T>::Insert(const T item, const BoundingBox& bounds) {
  Delete(item);
  return root.Insert(item, bounds, itemToNode);
}
template <typename T>
bool OctreeNode<T>::Insert(const T item, const BoundingBox& bounds, std::unordered_map<T, OctreeNode*>& itemToNode) {
  if (!Intersects(bounds))
    return false;
  items.insert(item);
  itemToNode[item] = this;
  if (Subdivide())
    // FIXME: not only insert this item, but also distribute existing items to children
    for (auto i = 0; i < 8; i++) {
      if (!children[i])
        continue;
      if (children[i]->Insert(item, bounds, itemToNode)) {
        items.erase(item);
        break;
      }
    }
  return true;
}
template <typename T>
bool OctreeNode<T>::Subdivide() {
  if (!leaf)
    return true;
  if (items.size() <= maxItems || depth >= maxDepth)
    return false;
  auto childExtent = extent * .5f;
  auto childDepth = depth + 1;
  auto childMinItems = std::max(1u, static_cast<unsigned int>(minItems / 8));
  auto childMaxItems = std::max(1u, static_cast<unsigned int>(maxItems / 8));
  for (auto i = 0; i < 8; ++i) {
    auto x = (i & 4) ? childExtent.x : -childExtent.x;
    auto y = (i & 2) ? childExtent.y : -childExtent.y;
    auto z = (i & 1) ? childExtent.z : -childExtent.z;
    auto offset = glm::vec3(x, y, z);
    auto childCenter = center + offset;
    auto childOctant = static_cast<Octant>(i);
    if (children[i])
      delete children[i];
    children[i] = new OctreeNode(childOctant, childCenter, childExtent, childDepth, maxDepth, childMinItems, childMaxItems);
  }
  leaf = false;
  return true;
}
template <typename T>
void OctreeNode<T>::Collapse(std::unordered_map<T, OctreeNode<T>*>& itemToNode) {
  if (!CanCollapse())
    return;
  for (auto i = 0; i < 8; ++i) {
    if (!children[i])
      continue;
    children[i]->Collapse(itemToNode);
    for (auto childItem : children[i]->items) {
      items.insert(childItem);
      itemToNode[childItem] = this;
    }
    delete children[i];
    children[i] = nullptr;
  }
  leaf = true;
}
template <typename T>
bool Octree<T>::Delete(const T item) {
  auto it = itemToNode.find(item);
  if (it == itemToNode.end())
    return false;
  it->second->items.erase(item);
  itemToNode.erase(item);
  root.Collapse(itemToNode);
  return true;
}
template <typename T>
void Octree<T>::Clear() {
  root.Clear(itemToNode);
}
template <typename T>
void OctreeNode<T>::Clear(std::unordered_map<T, OctreeNode*>& itemToNode) {
  for (auto i = 0; i < 8; ++i) {
    if (!children[i])
      continue;
    children[i]->Clear(itemToNode);
    delete children[i];
    children[i] = nullptr;
  }
  for (auto item : items)
    itemToNode.erase(item);
  items.clear();
  leaf = true;
}
template <typename T>
bool OctreeNode<T>::Intersects(const BoundingBox& other) {
  return (bounds.min.x <= other.min.x && bounds.max.x >= other.max.x) && (bounds.min.y <= other.min.y && bounds.max.y >= other.max.y) && (bounds.min.z <= other.min.z && bounds.max.z >= other.max.z);
}
template <typename T>
bool OctreeNode<T>::CanCollapse() const {
  if (leaf)
    return false;
  return GetCount() <= minItems;
}
template <typename T>
size_t Octree<T>::GetCount() const {
  return root.GetCount();
}
template <typename T>
size_t OctreeNode<T>::GetCount() const {
  auto total = items.size();
  for (auto i = 0; i < 8; i++) {
    if (!children[i])
      continue;
    total += children[i]->GetCount();
  }
  return total;
}
template <typename T>
std::string Octree<T>::ToString() const {
  std::ostringstream oss;
  root.InsertToStream(oss);
  return oss.str();
}
template <typename T>
void OctreeNode<T>::InsertToStream(std::ostringstream& oss) const {
  if (depth > 0)
    oss << std::endl;
  for (auto i = 0; i < depth; ++i)
    oss << "  ";
  oss << "(" << center.x << "," << center.y << "," << center.z << "),(" << extent.x << "," << extent.y << "," << extent.z << ")";
  for (const auto& item : items)
    oss << ":" << item.ToString();
  if (leaf)
    return;
  for (const auto& child : children)
    child->InsertToStream(oss);
}
} // namespace kuki
