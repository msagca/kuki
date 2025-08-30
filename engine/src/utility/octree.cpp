#include "component/mesh.hpp"
#include <utility/octree.hpp>
namespace kuki {
OctreeNode::OctreeNode(Octant octant, glm::vec3 center, glm::vec3 extent, unsigned int depth, unsigned int maxDepth, unsigned int minItems, unsigned int maxItems)
  : octant(octant), center(center), extent(extent), depth(depth), maxDepth(maxDepth), minItems(minItems), maxItems(maxItems), bounds(center - extent, center + extent) {}
OctreeNode::~OctreeNode() {
  for (auto i = 0; i < 8; ++i) {
    delete children[i];
    children[i] = nullptr;
  }
}
Octree::Octree(glm::vec3 center, glm::vec3 extent, unsigned int maxDepth, unsigned int minItems, unsigned int maxItems)
  : root(Octant::None, center, extent, 0, maxDepth, minItems, maxItems) {}
bool Octree::Insert(const unsigned int item, const BoundingBox& bounds) {
  Delete(item);
  return root.Insert(item, bounds, itemToNode);
}
bool OctreeNode::Insert(const unsigned int item, const BoundingBox& bounds, std::unordered_map<unsigned int, OctreeNode*>& itemToNode) {
  if (!Intersects(bounds))
    return false;
  items.insert(item);
  itemToNode[item] = this;
  if (Subdivide())
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
bool OctreeNode::Subdivide() {
  if (!leaf)
    return true;
  if (items.size() <= maxItems || depth >= maxDepth)
    return false;
  auto childExtent = extent * .5f;
  auto childDepth = depth + 1;
  auto childMinItems = std::max(1u, minItems / 8);
  auto childMaxItems = std::max(1u, maxItems / 8);
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
void OctreeNode::Collapse(std::unordered_map<unsigned int, OctreeNode*>& itemToNode) {
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
bool Octree::Delete(const unsigned int item) {
  auto it = itemToNode.find(item);
  if (it == itemToNode.end())
    return false;
  it->second->items.erase(item);
  itemToNode.erase(item);
  root.Collapse(itemToNode);
  return true;
}
void OctreeNode::Clear(std::unordered_map<unsigned int, OctreeNode*>& itemToNode) {
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
bool OctreeNode::Intersects(const BoundingBox& other) {
  return (bounds.min.x <= other.min.x && bounds.max.x >= other.max.x) && (bounds.min.y <= other.min.y && bounds.max.y >= other.max.y) && (bounds.min.z <= other.min.z && bounds.max.z >= other.max.z);
}
bool OctreeNode::CanCollapse() const {
  if (leaf)
    return false;
  return GetCount() <= minItems;
}
unsigned int Octree::GetCount() const {
  return root.GetCount();
}
unsigned int OctreeNode::GetCount() const {
  auto total = items.size();
  for (auto i = 0; i < 8; i++) {
    if (!children[i])
      continue;
    total += children[i]->GetCount();
  }
  return total;
}
std::string Octree::ToString() const {
  std::ostringstream oss;
  root.InsertToStream(oss);
  return oss.str();
}
void OctreeNode::InsertToStream(std::ostringstream& oss) const {
  if (depth > 0)
    oss << std::endl;
  for (auto i = 0; i < depth; ++i)
    oss << "  ";
  oss << "(" << center.x << "," << center.y << "," << center.z << "),(" << extent.x << "," << extent.y << "," << extent.z << ")";
  for (const auto& item : items)
    oss << ":" << item;
  if (leaf)
    return;
  for (const auto& child : children)
    child->InsertToStream(oss);
}
} // namespace kuki
