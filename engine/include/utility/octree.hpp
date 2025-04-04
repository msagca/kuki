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
  RightTopFront,
  None
};
/// @brief A generic octree class
/// @tparam T Type of the key used to store items in the octree
template <typename T>
class Octree {
private:
  const Octant octant;
  const glm::vec3 center;
  const glm::vec3 extent;
  const BoundingBox bounds;
  const unsigned int depth;
  const unsigned int maxDepth;
  const unsigned int minItems;
  const unsigned int maxItems;
  bool leaf;
  std::unordered_map<T, BoundingBox> items;
  Octree* parent;
  Octree* children[8];
  Octree(Octant, glm::vec3, glm::vec3, unsigned int, unsigned int, unsigned int, unsigned int);
  bool InsertToChildren(const T&, const BoundingBox&);
  bool InsertNoSubdivide(const T&, const BoundingBox&);
  bool DistributeToChildren(const T&, const BoundingBox&);
  bool Subdivide();
  void Distribute();
  bool Overlaps(const BoundingBox&);
  bool Contains(const BoundingBox&);
  /// @return true if total number of items in children is not greater than minItems, false otherwise
  bool CanCollapse() const;
  /// @brief Merge items of children into the current node, then remove the children
  void Collapse();
  void InsertToStream(std::ostringstream&) const;
  template <typename F>
  void ForEachInFrustumInternal(const Camera& camera, F func, std::unordered_set<T>& visited);
public:
  /// @brief
  /// @param center Center of the octree
  /// @param extent Extent of the octree
  /// @param maxDepth Maximum depth of the octree
  /// @param minItems Minimum number of items in a node before it can be merged
  /// @param maxItems Maximum number of items in a node before it can be subdivided
  Octree(glm::vec3 = glm::vec3{.0f}, glm::vec3 = glm::vec3{10.0f}, unsigned int = 4, unsigned int = 16, unsigned int = 128);
  Octant GetOctant() const;
  unsigned int GetDepth() const;
  unsigned int GetMaxDepth() const;
  glm::vec3 GetCenter() const;
  glm::vec3 GetExtent() const;
  BoundingBox GetBounds() const;
  unsigned int GetCount() const;
  bool IsLeaf() const;
  /// @brief Insert an item into the octree
  /// @param item Key to store the item
  /// @param bounds Bounding box of the item
  /// @return true if the item was inserted, false otherwise
  bool Insert(const T&, const BoundingBox&);
  bool Delete(const T&);
  /// @brief Delete all octree nodes (except the root node)
  void Clear();
  std::string ToString() const;
  /// @brief Execute a function on each node in the octree
  template <typename F>
  void ForEach(F);
  template <typename F>
  void ForEachLeaf(F);
  template <typename F>
  void ForEachInFrustum(const Camera&, F);
};
template <typename T>
Octree<T>::Octree(Octant octant, glm::vec3 center, glm::vec3 extent, unsigned int depth, unsigned int maxDepth, unsigned int minItems, unsigned int maxItems)
  : octant(octant), center(center), extent(extent), depth(depth), maxDepth(maxDepth), minItems(minItems), maxItems(maxItems), bounds(center - extent, center + extent), leaf(true) {
  assert(extent.x > 0 && extent.y > 0 && extent.z > 0 && "Extent must be greater than zero in all axes.");
  assert(maxDepth >= depth && "maxDepth must be greater than or equal to depth.");
}
template <typename T>
Octree<T>::Octree(glm::vec3 center, glm::vec3 extent, unsigned int maxDepth, unsigned int minItems, unsigned int maxItems)
  : Octree(Octant::None, center, extent, 0, maxDepth, minItems, maxItems) {}
template <typename T>
Octant Octree<T>::GetOctant() const {
  return octant;
}
template <typename T>
unsigned int Octree<T>::GetDepth() const {
  return depth;
}
template <typename T>
unsigned int Octree<T>::GetMaxDepth() const {
  return maxDepth;
}
template <typename T>
glm::vec3 Octree<T>::GetCenter() const {
  return center;
}
template <typename T>
glm::vec3 Octree<T>::GetExtent() const {
  return extent;
}
template <typename T>
BoundingBox Octree<T>::GetBounds() const {
  return bounds;
}
template <typename T>
unsigned int Octree<T>::GetCount() const {
  auto total = items.size();
  if (!leaf)
    for (auto i = 0; i < 8; i++)
      total += children[i]->GetCount();
  return total;
}
template <typename T>
bool Octree<T>::IsLeaf() const {
  return leaf;
}
template <typename T>
bool Octree<T>::Insert(const T& item, const BoundingBox& bounds) {
  if (!Contains(bounds)) {
    Delete(item); // NOTE: if this is an existing item and went out of bounds, remove it from the octree
    return false;
  }
  items[item] = bounds;
  if (!leaf && InsertToChildren(item, bounds))
    items.erase(item);
  if (leaf && Subdivide())
    Distribute();
  return true;
}
template <typename T>
bool Octree<T>::InsertToChildren(const T& item, const BoundingBox& bounds) {
  for (auto i = 0; i < 8; ++i)
    if (children[i]->Insert(item, bounds))
      return true;
  return false;
}
template <typename T>
bool Octree<T>::Subdivide() {
  if (items.size() <= maxItems || depth >= maxDepth)
    return false;
  leaf = false;
  auto childExtent = extent * .5f;
  auto childDepth = depth + 1;
  auto childMinItems = minItems / 8;
  auto childMaxItems = maxItems / 8;
  for (auto i = 0; i < 8; ++i) {
    auto x = (i & 4) != 0 ? childExtent.x : -childExtent.x;
    auto y = (i & 2) != 0 ? childExtent.y : -childExtent.y;
    auto z = (i & 1) != 0 ? childExtent.z : -childExtent.z;
    glm::vec3 offset(x, y, z);
    auto childCenter = center + offset;
    auto childOctant = static_cast<Octant>(i);
    children[i] = new Octree<T>(childOctant, childCenter, childExtent, childDepth, maxDepth, childMinItems, childMaxItems);
    children[i]->parent = this;
  }
  return true;
}
template <typename T>
void Octree<T>::Distribute() {
  std::unordered_set<T> toDelete;
  for (const auto& [key, val] : items)
    if (DistributeToChildren(key, val))
      toDelete.insert(key);
  for (const auto& key : toDelete)
    items.erase(key);
}
template <typename T>
bool Octree<T>::DistributeToChildren(const T& item, const BoundingBox& bounds) {
  for (auto i = 0; i < 8; ++i)
    if (children[i]->InsertNoSubdivide(item, bounds))
      return true;
  return false;
}
template <typename T>
bool Octree<T>::InsertNoSubdivide(const T& item, const BoundingBox& bounds) {
  if (!Contains(bounds))
    return false;
  items[item] = bounds;
  if (!leaf && DistributeToChildren(item, bounds))
    items.erase(item);
  return true;
}
template <typename T>
bool Octree<T>::CanCollapse() const {
  if (leaf)
    return false;
  return GetCount() <= minItems;
}
template <typename T>
void Octree<T>::Collapse() {
  if (!CanCollapse())
    return;
  for (auto i = 0; i < 8; ++i) {
    if (!children[i]->leaf)
      children[i]->Collapse();
    // NOTE: insert does not overwrite existing items
    items.insert(children[i]->items.begin(), children[i]->items.end());
  }
  Clear();
  leaf = true;
}
template <typename T>
bool Octree<T>::Overlaps(const BoundingBox& other) {
  return (bounds.min.x <= other.max.x && bounds.max.x >= other.min.x) && (bounds.min.y <= other.max.y && bounds.max.y >= other.min.y) && (bounds.min.z <= other.max.z && bounds.max.z >= other.min.z);
}
template <typename T>
bool Octree<T>::Contains(const BoundingBox& other) {
  return (bounds.min.x <= other.min.x && bounds.max.x >= other.max.x) && (bounds.min.y <= other.min.y && bounds.max.y >= other.max.y) && (bounds.min.z <= other.min.z && bounds.max.z >= other.max.z);
}
template <typename T>
bool Octree<T>::Delete(const T& item) {
  auto deleted = items.erase(item);
  if (!deleted && !leaf)
    for (auto i = 0; i < 8; ++i)
      if (children[i]->Delete(item)) {
        deleted = true;
        break;
      }
  if (deleted)
    Collapse();
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
void Octree<T>::ForEach(F func) {
  func(this);
  if (leaf)
    return;
  for (auto i = 0; i < 8; ++i)
    children[i]->ForEach(func);
}
template <typename T>
template <typename F>
void Octree<T>::ForEachLeaf(F func) {
  if (leaf) {
    func(this, static_cast<Octant>(octant));
    return;
  }
  for (auto i = 0; i < 8; ++i)
    children[i]->ForEachLeaf(func);
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
  for (const auto& [key, _] : items)
    if (visited.find(key) == visited.end()) {
      visited.insert(key);
      func(key);
    }
  if (!leaf)
    for (auto i = 0; i < 8; ++i)
      children[i]->ForEachInFrustumInternal(camera, func, visited);
}
