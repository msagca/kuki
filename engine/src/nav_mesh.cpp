#include <algorithm>
#include <glm/geometric.hpp>
#include <nav_mesh.hpp>
#include <queue>
namespace kuki {
namespace {
  /// @brief Twice the signed area of a triangle: above nought when `c` is to the left of the line
  /// from `a` to `b`, with x to the right and y up.
  auto Cross(const glm::vec2 &a, const glm::vec2 &b, const glm::vec2 &c) -> float {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
  }
  auto Same(const glm::vec2 &a, const glm::vec2 &b) -> bool {
    const auto d = a - b;
    return d.x * d.x + d.y * d.y < 1e-10f;
  }
} // namespace
NavMesh::NavMesh(std::vector<glm::vec2> vertices, std::vector<std::vector<uint32_t>> list)
  : polygons(std::move(vertices), std::move(list)), costs(polygons.CellCount(), 1.f), blocked(polygons.CellCount(), false) {}
auto NavMesh::FromGrid(const IrregularGrid &grid) -> NavMesh {
  NavMesh mesh;
  mesh.polygons = grid;
  mesh.costs.assign(grid.CellCount(), 1.f);
  mesh.blocked.assign(grid.CellCount(), false);
  return mesh;
}
auto NavMesh::PolygonCount() const -> size_t {
  return polygons.CellCount();
}
auto NavMesh::GetPolygons() const -> const IrregularGrid & {
  return polygons;
}
auto NavMesh::SetBlocked(const uint32_t polygon, const bool value) -> void {
  if (polygon < blocked.size())
    blocked[polygon] = value;
}
auto NavMesh::IsBlocked(const uint32_t polygon) const -> bool {
  return polygon >= blocked.size() || blocked[polygon];
}
auto NavMesh::SetCost(const uint32_t polygon, const float cost) -> void {
  if (polygon >= costs.size())
    return;
  costs[polygon] = std::max(cost, .01f);
  // The heuristic has to underestimate to keep A* finding the cheapest way, so it reckons the rest
  // of the distance at the cheapest any polygon costs.
  cheapest = *std::ranges::min_element(costs);
}
auto NavMesh::GetCost(const uint32_t polygon) const -> float {
  return polygon < costs.size() ? costs[polygon] : 1.f;
}
auto NavMesh::Locate(const glm::vec2 &point) const -> uint32_t {
  return polygons.Locate(point);
}
auto NavMesh::FindCorridor(const uint32_t from, const uint32_t to) const -> std::vector<uint32_t> {
  const auto count = polygons.CellCount();
  if (from >= count || to >= count || IsBlocked(from) || IsBlocked(to))
    return {};
  if (from == to)
    return {from};
  const auto goal = polygons.GetCell(to).centroid;
  std::vector<float> best(count, std::numeric_limits<float>::max());
  std::vector<uint32_t> previous(count, INVALID_CELL);
  using Entry = std::pair<float, uint32_t>;
  std::priority_queue<Entry, std::vector<Entry>, std::greater<>> open;
  best[from] = .0f;
  open.emplace(glm::distance(polygons.GetCell(from).centroid, goal) * cheapest, from);
  while (!open.empty()) {
    const auto [estimate, polygon] = open.top();
    open.pop();
    if (polygon == to)
      break;
    const auto &cell = polygons.GetCell(polygon);
    if (estimate - glm::distance(cell.centroid, goal) * cheapest > best[polygon] + 1e-4f)
      continue;
    for (const auto next : cell.neighbours) {
      if (blocked[next])
        continue;
      const auto &other = polygons.GetCell(next);
      // Half the way in each polygon, at each one's own cost.
      const auto step = glm::distance(cell.centroid, other.centroid) * .5f * (costs[polygon] + costs[next]);
      if (const auto total = best[polygon] + step; total < best[next]) {
        best[next] = total;
        previous[next] = polygon;
        open.emplace(total + glm::distance(other.centroid, goal) * cheapest, next);
      }
    }
  }
  if (previous[to] == INVALID_CELL)
    return {};
  std::vector<uint32_t> corridor{to};
  while (corridor.back() != from)
    corridor.push_back(previous[corridor.back()]);
  std::ranges::reverse(corridor);
  return corridor;
}
auto NavMesh::Portals(const std::span<const uint32_t> corridor) const -> std::vector<NavPortal> {
  std::vector<NavPortal> portals;
  for (size_t i = 0; i + 1 < corridor.size(); ++i) {
    const auto &cell = polygons.GetCell(corridor[i]);
    for (size_t side = 0; side < cell.across.size(); ++side)
      if (cell.across[side] == corridor[i + 1]) {
        // The polygon is counter-clockwise, so its inside is to the left of every side. Standing in
        // it facing the side, the side's start is on the right and its end on the left.
        const auto right = polygons.GetCorner(cell.corners[side]);
        const auto left = polygons.GetCorner(cell.corners[(side + 1) % cell.corners.size()]);
        portals.push_back({left, right});
        break;
      }
  }
  return portals;
}
auto NavMesh::StringPull(const glm::vec2 &from, const glm::vec2 &to, const std::span<const uint32_t> corridor, const float clearance) const -> std::vector<glm::vec2> {
  // The simple stupid funnel, after Mononen: the funnel's two sides are the lines from its apex to
  // the left and right ends of the sides crossed so far. Each side crossed next narrows it where it
  // can; where the right would swing past the left, the left end is a corner the path turns at, it
  // becomes the apex, and the funnel starts again from there -- and the same the other way round.
  std::vector<NavPortal> portals{{from, from}};
  for (auto portal : Portals(corridor)) {
    if (clearance > .0f) {
      const auto span = portal.left - portal.right;
      const auto width = glm::length(span);
      if (width <= 2.f * clearance) {
        portal.left = portal.right = (portal.left + portal.right) * .5f;
      } else {
        const auto inward = span / width * clearance;
        portal.left -= inward;
        portal.right += inward;
      }
    }
    portals.push_back(portal);
  }
  portals.push_back({to, to});
  std::vector<glm::vec2> path{from};
  auto apex = from;
  auto left = portals[0].left;
  auto right = portals[0].right;
  size_t apexIndex = 0;
  size_t leftIndex = 0;
  size_t rightIndex = 0;
  const auto Turn = [&](const glm::vec2 &corner) {
    if (!Same(path.back(), corner))
      path.push_back(corner);
  };
  for (size_t i = 1; i < portals.size(); ++i) {
    const auto &nextLeft = portals[i].left;
    const auto &nextRight = portals[i].right;
    if (Cross(apex, right, nextRight) >= .0f) {
      if (Same(apex, right) || Cross(apex, left, nextRight) < .0f) {
        right = nextRight;
        rightIndex = i;
      } else {
        Turn(left);
        apex = left;
        apexIndex = leftIndex;
        left = right = apex;
        leftIndex = rightIndex = apexIndex;
        i = apexIndex;
        continue;
      }
    }
    if (Cross(apex, left, nextLeft) <= .0f) {
      if (Same(apex, left) || Cross(apex, right, nextLeft) > .0f) {
        left = nextLeft;
        leftIndex = i;
      } else {
        Turn(right);
        apex = right;
        apexIndex = rightIndex;
        left = right = apex;
        leftIndex = rightIndex = apexIndex;
        i = apexIndex;
        continue;
      }
    }
  }
  Turn(to);
  return path;
}
auto NavMesh::FindPath(const glm::vec2 &from, const glm::vec2 &to, const float clearance) const -> std::vector<glm::vec2> {
  const auto start = Locate(from);
  const auto end = Locate(to);
  if (start == INVALID_CELL || end == INVALID_CELL)
    return {};
  const auto corridor = FindCorridor(start, end);
  if (corridor.empty())
    return {};
  return StringPull(from, to, corridor, clearance);
}
} // namespace kuki
