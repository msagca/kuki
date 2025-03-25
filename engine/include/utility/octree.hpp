#pragma once
#include <component/camera.hpp>
#include <component/mesh.hpp>
#include <glm/ext/vector_float3.hpp>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
enum class Octant : unsigned int {
  LeftBottomBack,
  LeftBottomFront,
  LeftTopBack,
  LeftTopFront,
  RightBottomBack,
  RightBottomFront,
  RightTopBack,
  RightTopFront
};
/// @brief A generic octree class
/// @tparam T Type of the key used to store items in the octree
template <typename T>
class Octree {
private:
  const glm::vec3 center;
  const glm::vec3 extent;
  const BoundingBox bounds;
  const unsigned int depth;
  const unsigned int maxDepth;
  const unsigned int minItems;
  const unsigned int maxItems;
  bool leaf = true;
  std::unordered_map<T, BoundingBox> items;
  Octree* children[8];
  bool Subdivide();
  bool Overlaps(const BoundingBox&);
  /// @return true if total number of items in children is not greater than minItems, false otherwise
  bool ShouldMerge() const;
  /// @brief Merge items of children into the current node, then remove the children
  void MergeChildren();
  void InsertToStream(std::ostringstream&) const;
  template <typename F>
  void ForEachInFrustumInternal(const Camera& camera, F func, std::unordered_set<T>& visited);
public:
  /// @brief
  /// @param center Center of the octree
  /// @param extent Extent of the octree
  /// @param depth Depth level of the current node
  /// @param maxDepth Maximum depth of the octree
  /// @param minItems Minimum number of items in a node before it can be merged
  /// @param maxItems Maximum number of items in a node before it can be subdivided
  Octree(glm::vec3 = glm::vec3{.0f}, glm::vec3 = glm::vec3{10.0f}, unsigned int = 0, unsigned int = 5, unsigned int = 10, unsigned int = 100);
  BoundingBox GetBounds() const;
  unsigned int GetItemCount() const;
  bool IsLeaf() const;
  /// @brief Insert an item into the octree
  /// @param item Key to store the item
  /// @param bounds Bounding box of the item
  /// @return true if the item was inserted, false otherwise
  bool Insert(T, const BoundingBox&);
  bool Delete(T);
  /// @brief Delete all octree nodes (except the root node)
  void Clear();
  std::string ToString() const;
  template <typename F>
  void ForEachInFrustum(const Camera&, F);
};
template <typename T>
Octree<T>::Octree(glm::vec3 center, glm::vec3 extent, unsigned int depth, unsigned int maxDepth, unsigned int minItems, unsigned int maxItems)
  : center(center), extent(extent), depth(depth), maxDepth(maxDepth), minItems(minItems), maxItems(maxItems), bounds(center - extent, center + extent) {}
template <typename T>
BoundingBox Octree<T>::GetBounds() const {
  return bounds;
}
template <typename T>
unsigned int Octree<T>::GetItemCount() const {
  return items.size();
}
template <typename T>
bool Octree<T>::IsLeaf() const {
  return leaf;
}
template <typename T>
bool Octree<T>::Insert(T item, const BoundingBox& bounds) {
  if (!Overlaps(bounds))
    return false;
  if (leaf) {
    items[item] = bounds;
    if (items.size() > maxItems && Subdivide()) {
      for (const auto& [key, val] : items)
        // distribute items to children
        for (auto i = 0; i < 8; ++i)
          children[i]->Insert(key, val);
      // TODO: make sure each item is inserted into at least one child node
      items.clear();
    }
  } else
    for (auto i = 0; i < 8; ++i)
      children[i]->Insert(item, bounds);
  return true;
}
template <typename T>
bool Octree<T>::Subdivide() {
  if (maxDepth <= depth)
    return false;
  leaf = false;
  auto childExtent = extent * .5f;
  auto childDepth = depth + 1;
  for (auto i = 0; i < 8; ++i) {
    auto x = (i & 4) != 0 ? .5f : -.5f;
    auto y = (i & 2) != 0 ? .5f : -.5f;
    auto z = (i & 1) != 0 ? .5f : -.5f;
    glm::vec3 offset(x, y, z);
    auto childCenter = center + offset * extent;
    children[i] = new Octree<T>(childCenter, childExtent, childDepth);
  }
  return true;
}
template <typename T>
bool Octree<T>::ShouldMerge() const {
  auto total = 0;
  for (auto i = 0; i < 8; ++i) {
    if (!children[i]->IsLeaf())
      return false;
    total += children[i]->GetItemCount();
  }
  return total <= minItems;
}
template <typename T>
void Octree<T>::MergeChildren() {
  for (auto i = 0; i < 8; ++i)
    if (children[i]->IsLeaf())
      // NOTE: insert does not overwrite existing items
      items.insert(children[i]->items.begin(), children[i]->items.end());
  Clear();
  leaf = true;
}
template <typename T>
bool Octree<T>::Overlaps(const BoundingBox& other) {
  return (bounds.min.x <= other.max.x && bounds.max.x >= other.min.x) && (bounds.min.y <= other.max.y && bounds.max.y >= other.min.y) && (bounds.min.z <= other.max.z && bounds.max.z >= other.min.z);
}
template <typename T>
bool Octree<T>::Delete(T item) {
  if (leaf)
    return items.erase(item);
  auto deleted = false;
  for (auto i = 0; i < 8; ++i)
    if (children[i]->Delete(item))
      deleted = true;
  if (deleted && ShouldMerge())
    MergeChildren();
  return deleted;
}
template <typename T>
void Octree<T>::Clear() {
  if (leaf)
    return;
  for (auto i = 0; i < 8; ++i) {
    children[i]->Clear();
    delete children[i];
  }
}
template <typename T>
std::string Octree<T>::ToString() const {
  std::ostringstream oss;
  InsertToStream(oss);
  return oss.str();
}
template <typename T>
void Octree<T>::InsertToStream(std::ostringstream& oss) const {
  if (depth > 0)
    oss << std::endl;
  for (auto i = 0; i < depth; ++i)
    oss << "  ";
  oss << "(" << center.x << "," << center.y << "," << center.z << "),(" << extent.x << "," << extent.y << "," << extent.z << ")";
  for (const auto& [key, _] : items)
    oss << ":" << key;
  if (leaf)
    return;
  for (const auto& child : children)
    child->InsertToStream(oss);
}
template <typename T>
template <typename F>
void Octree<T>::ForEachInFrustum(const Camera& camera, F func) {
  std::unordered_set<T> visited;
  ForEachInFrustumInternal(camera, func, visited);
}
template <typename T>
template <typename F>
void Octree<T>::ForEachInFrustumInternal(const Camera& camera, F func, std::unordered_set<T>& visited) {
  if (!camera.OverlapsFrustum(bounds))
    return;
  if (leaf) {
    for (const auto& [key, _] : items)
      if (visited.find(key) == visited.end()) {
        visited.insert(key);
        func(key);
      }
  } else
    for (auto i = 0; i < 8; ++i)
      children[i]->ForEachInFrustumInternal(camera, func, visited);
}
