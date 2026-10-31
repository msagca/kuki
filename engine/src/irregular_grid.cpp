#include <algorithm>
#include <array>
#include <cmath>
#include <glm/ext/vector_double2.hpp>
#include <glm/geometric.hpp>
#include <irregular_grid.hpp>
#include <numeric>
#include <random.hpp>
#include <unordered_map>
namespace kuki {
namespace {
  constexpr uint32_t NONE = std::numeric_limits<uint32_t>::max();
  /// @brief Points scattered over a rectangle no two closer than `radius`, and none of it left
  /// bare: Bridson's sampling, which keeps a list of points still able to have a neighbour placed
  /// beside them and tries a few places round one at a time until none can.
  ///
  /// Random points would do for a Voronoi diagram, and make one of slivers and giants. Points kept
  /// apart like this make cells that are already about one size before any relaxation, so the
  /// relaxation has less to do and can stop before the cells turn into a honeycomb.
  auto ScatterPoints(const glm::dvec2 &low, const glm::dvec2 &high, const double radius, Random &random) -> std::vector<glm::dvec2> {
    constexpr int ATTEMPTS = 24;
    const auto cellSize = radius / std::sqrt(2.);
    const auto size = high - low;
    const auto columns = std::max(1, static_cast<int>(std::ceil(size.x / cellSize)));
    const auto rows = std::max(1, static_cast<int>(std::ceil(size.y / cellSize)));
    std::vector<uint32_t> lattice(static_cast<size_t>(columns) * rows, NONE);
    std::vector<glm::dvec2> points;
    std::vector<uint32_t> active;
    const auto Slot = [&](const glm::dvec2 &p) {
      return glm::ivec2(std::clamp(static_cast<int>((p.x - low.x) / cellSize), 0, columns - 1), std::clamp(static_cast<int>((p.y - low.y) / cellSize), 0, rows - 1));
    };
    const auto Place = [&](const glm::dvec2 &p) {
      const auto slot = Slot(p);
      lattice[static_cast<size_t>(slot.y) * columns + slot.x] = static_cast<uint32_t>(points.size());
      active.push_back(static_cast<uint32_t>(points.size()));
      points.push_back(p);
    };
    const auto Fits = [&](const glm::dvec2 &p) {
      if (p.x < low.x || p.y < low.y || p.x >= high.x || p.y >= high.y)
        return false;
      const auto slot = Slot(p);
      for (auto y = std::max(0, slot.y - 2); y <= std::min(rows - 1, slot.y + 2); ++y)
        for (auto x = std::max(0, slot.x - 2); x <= std::min(columns - 1, slot.x + 2); ++x)
          if (const auto other = lattice[static_cast<size_t>(y) * columns + x]; other != NONE) {
            const auto d = points[other] - p;
            if (d.x * d.x + d.y * d.y < radius * radius)
              return false;
          }
      return true;
    };
    Place(low + size * glm::dvec2(random.Float(), random.Float()));
    while (!active.empty()) {
      const auto pick = random.Below(static_cast<uint32_t>(active.size()));
      const auto centre = points[active[pick]];
      auto placed = false;
      for (auto attempt = 0; attempt < ATTEMPTS && !placed; ++attempt) {
        const auto angle = static_cast<double>(random.Float()) * 6.283185307179586;
        const auto distance = radius * (1. + static_cast<double>(random.Float()));
        const auto candidate = centre + glm::dvec2(std::cos(angle), std::sin(angle)) * distance;
        if (Fits(candidate)) {
          Place(candidate);
          placed = true;
        }
      }
      if (!placed) {
        active[pick] = active.back();
        active.pop_back();
      }
    }
    return points;
  }
  struct Triangle {
    std::array<uint32_t, 3> v{};
    glm::dvec2 centre{};
    double radius2{};
  };
  auto MakeTriangle(const std::vector<glm::dvec2> &points, const uint32_t a, uint32_t b, uint32_t c) -> Triangle {
    const auto &pa = points[a];
    const auto &pb = points[b];
    const auto &pc = points[c];
    // Counter-clockwise, which the fan walk below depends on.
    if ((pb.x - pa.x) * (pc.y - pa.y) - (pb.y - pa.y) * (pc.x - pa.x) < .0)
      std::swap(b, c);
    Triangle triangle{{a, b, c}};
    const auto &p0 = points[triangle.v[0]];
    const auto &p1 = points[triangle.v[1]];
    const auto &p2 = points[triangle.v[2]];
    const auto d = 2. * (p0.x * (p1.y - p2.y) + p1.x * (p2.y - p0.y) + p2.x * (p0.y - p1.y));
    if (std::abs(d) < 1e-12) {
      // Three points in a line have no circle through them. Such a triangle is only ever made with
      // the super triangle's far corners, and a circle that contains everything keeps it from being
      // kept by mistake.
      triangle.centre = (p0 + p1 + p2) / 3.;
      triangle.radius2 = std::numeric_limits<double>::max();
      return triangle;
    }
    const auto s0 = p0.x * p0.x + p0.y * p0.y;
    const auto s1 = p1.x * p1.x + p1.y * p1.y;
    const auto s2 = p2.x * p2.x + p2.y * p2.y;
    triangle.centre = {(s0 * (p1.y - p2.y) + s1 * (p2.y - p0.y) + s2 * (p0.y - p1.y)) / d, (s0 * (p2.x - p1.x) + s1 * (p0.x - p2.x) + s2 * (p1.x - p0.x)) / d};
    const auto r = p0 - triangle.centre;
    triangle.radius2 = r.x * r.x + r.y * r.y;
    return triangle;
  }
  /// @brief The Delaunay triangulation of a set of points, by Bowyer and Watson's method with
  /// Bourke's sweep.
  ///
  /// Points are added one at a time; each removes the triangles whose circles it falls inside and
  /// fills the hole with a fan to itself. Taken in order along x, a triangle whose circle ends to the
  /// left of the point being added can never be touched again and is put aside, which keeps the list
  /// searched for each point short -- the difference between seconds and milliseconds at a few
  /// thousand points.
  ///
  /// Three points far outside are added first, and the triangles touching them are kept in the
  /// answer: they are how a caller tells a point on the hull, whose Voronoi cell has no end.
  auto Triangulate(std::vector<glm::dvec2> points) -> std::pair<std::vector<Triangle>, uint32_t> {
    const auto count = static_cast<uint32_t>(points.size());
    glm::dvec2 low{std::numeric_limits<double>::max()};
    glm::dvec2 high{std::numeric_limits<double>::lowest()};
    for (const auto &p : points) {
      low = glm::min(low, p);
      high = glm::max(high, p);
    }
    const auto middle = (low + high) * .5;
    const auto reach = std::max(high.x - low.x, high.y - low.y) + 1.;
    points.push_back(middle + glm::dvec2(-20. * reach, -reach));
    points.push_back(middle + glm::dvec2(.0, 20. * reach));
    points.push_back(middle + glm::dvec2(20. * reach, -reach));
    std::vector<uint32_t> order(count);
    std::iota(order.begin(), order.end(), 0u);
    std::ranges::sort(order, [&](const uint32_t a, const uint32_t b) { return points[a].x < points[b].x || (points[a].x == points[b].x && points[a].y < points[b].y); });
    std::vector<Triangle> open{MakeTriangle(points, count, count + 1, count + 2)};
    std::vector<Triangle> closed;
    std::vector<std::pair<uint32_t, uint32_t>> rim;
    for (const auto index : order) {
      const auto &p = points[index];
      rim.clear();
      for (size_t i = open.size(); i-- > 0;) {
        const auto &triangle = open[i];
        const auto dx = p.x - triangle.centre.x;
        if (dx > .0 && dx * dx > triangle.radius2) {
          closed.push_back(triangle);
          open[i] = open.back();
          open.pop_back();
          continue;
        }
        const auto d = p - triangle.centre;
        if (d.x * d.x + d.y * d.y < triangle.radius2) {
          for (auto k = 0; k < 3; ++k) {
            const auto a = triangle.v[k];
            const auto b = triangle.v[(k + 1) % 3];
            rim.emplace_back(std::min(a, b), std::max(a, b));
          }
          open[i] = open.back();
          open.pop_back();
        }
      }
      // A side two removed triangles shared is inside the hole and goes; one only one of them had is
      // the hole's rim, and each is joined to the new point.
      std::ranges::sort(rim);
      for (size_t i = 0; i < rim.size();) {
        auto j = i + 1;
        while (j < rim.size() && rim[j] == rim[i])
          ++j;
        if (j - i == 1)
          open.push_back(MakeTriangle(points, rim[i].first, rim[i].second, index));
        i = j;
      }
    }
    closed.insert(closed.end(), open.begin(), open.end());
    return {std::move(closed), count};
  }
  /// @brief The Voronoi cells of a triangulation's points: for each point, the centres of the
  /// triangles round it, counter-clockwise, and the point across each side; or nothing for a point
  /// on the hull, whose cell is open.
  struct Fan {
    std::vector<uint32_t> triangles;
    std::vector<uint32_t> across;
  };
  auto BuildFans(const std::vector<Triangle> &triangles, const uint32_t count) -> std::vector<std::optional<Fan>> {
    // Per point, each triangle round it as (the point after it, the point after that, the triangle).
    struct Spoke {
      uint32_t next;
      uint32_t after;
      uint32_t triangle;
    };
    std::vector<std::vector<Spoke>> spokes(count);
    std::vector<bool> open(count, false);
    for (uint32_t t = 0; t < triangles.size(); ++t) {
      const auto &v = triangles[t].v;
      const auto outer = v[0] >= count || v[1] >= count || v[2] >= count;
      for (auto k = 0; k < 3; ++k) {
        const auto s = v[k];
        if (s >= count)
          continue;
        if (outer)
          open[s] = true;
        else
          spokes[s].push_back({v[(k + 1) % 3], v[(k + 2) % 3], t});
      }
    }
    std::vector<std::optional<Fan>> fans(count);
    for (uint32_t s = 0; s < count; ++s) {
      if (open[s] || spokes[s].size() < 3)
        continue;
      // Round the point counter-clockwise: the triangle (s, u, w) is followed by the one that starts
      // (s, w, ...). Walked by who follows whom rather than sorted by angle, because two triangles
      // whose centres coincide sort either way round and only one way is a polygon.
      Fan fan;
      const auto first = spokes[s].front().next;
      auto key = first;
      auto closed = false;
      for (size_t step = 0; step <= spokes[s].size(); ++step) {
        const auto it = std::ranges::find(spokes[s], key, &Spoke::next);
        if (it == spokes[s].end())
          break;
        fan.triangles.push_back(it->triangle);
        fan.across.push_back(it->after);
        key = it->after;
        if (key == first) {
          closed = true;
          break;
        }
      }
      if (closed && fan.triangles.size() == spokes[s].size())
        fans[s] = std::move(fan);
    }
    return fans;
  }
  auto PolygonCentroid(const std::vector<glm::dvec2> &polygon) -> std::optional<glm::dvec2> {
    auto area = .0;
    glm::dvec2 sum{};
    for (size_t i = 0; i < polygon.size(); ++i) {
      const auto &a = polygon[i];
      const auto &b = polygon[(i + 1) % polygon.size()];
      const auto cross = a.x * b.y - b.x * a.y;
      area += cross;
      sum += (a + b) * cross;
    }
    if (std::abs(area) < 1e-12)
      return std::nullopt;
    return sum / (3. * area);
  }
  auto EdgeKey(const uint32_t a, const uint32_t b) -> uint64_t {
    return static_cast<uint64_t>(std::min(a, b)) << 32u | std::max(a, b);
  }
  /// @brief Shrinks sides to corners, as `GridSettings::shortSide` and `maxNeighbours` ask, and
  /// drops the corners no cell uses any more.
  auto CollapseSides(std::vector<glm::vec2> &corners, std::vector<std::vector<uint32_t>> &polygons, const GridSettings &settings) -> void {
    struct Side {
      uint32_t a;
      uint32_t b;
      uint32_t left{NONE};
      uint32_t right{NONE};
      float length{};
    };
    std::unordered_map<uint64_t, uint32_t> lookup;
    std::vector<Side> sides;
    for (uint32_t c = 0; c < polygons.size(); ++c) {
      const auto &polygon = polygons[c];
      for (size_t i = 0; i < polygon.size(); ++i) {
        const auto a = polygon[i];
        const auto b = polygon[(i + 1) % polygon.size()];
        const auto [it, added] = lookup.try_emplace(EdgeKey(a, b), static_cast<uint32_t>(sides.size()));
        if (added)
          sides.push_back({a, b, c, NONE, glm::distance(corners[a], corners[b])});
        else
          sides[it->second].right = c;
      }
    }
    std::vector<uint32_t> neighbours(polygons.size(), 0u);
    for (const auto &side : sides)
      if (side.left != NONE && side.right != NONE) {
        ++neighbours[side.left];
        ++neighbours[side.right];
      }
    std::vector<uint32_t> parent(corners.size());
    std::iota(parent.begin(), parent.end(), 0u);
    std::vector<bool> touched(corners.size(), false);
    const auto Merge = [&](const Side &side) {
      if (touched[side.a] || touched[side.b])
        return false;
      const auto shared = side.left != NONE && side.right != NONE;
      if (shared && (neighbours[side.left] <= settings.minNeighbours || neighbours[side.right] <= settings.minNeighbours))
        return false;
      // A cell is a polygon of three sides at the least, and shrinking one of a triangle's would
      // leave it a line.
      if ((side.left != NONE && polygons[side.left].size() <= 3) || (side.right != NONE && polygons[side.right].size() <= 3))
        return false;
      if (shared) {
        --neighbours[side.left];
        --neighbours[side.right];
      }
      corners[side.a] = (corners[side.a] + corners[side.b]) * .5f;
      parent[side.b] = side.a;
      touched[side.a] = touched[side.b] = true;
      return true;
    };
    std::vector<uint32_t> order(sides.size());
    std::iota(order.begin(), order.end(), 0u);
    std::ranges::sort(order, [&](const uint32_t x, const uint32_t y) { return sides[x].length < sides[y].length; });
    const auto threshold = settings.shortSide * settings.spacing;
    for (const auto index : order) {
      if (sides[index].length >= threshold)
        break;
      Merge(sides[index]);
    }
    // Then the cells with too many neighbours, each giving up its shortest shared sides first. The
    // corners a first shrink touched are left alone in this round too, so no corner moves twice and
    // no side shrinks into a neighbour's.
    std::vector<std::vector<uint32_t>> ownSides(polygons.size());
    for (const auto index : order)
      if (sides[index].left != NONE && sides[index].right != NONE) {
        ownSides[sides[index].left].push_back(index);
        ownSides[sides[index].right].push_back(index);
      }
    for (uint32_t c = 0; c < polygons.size(); ++c)
      for (const auto index : ownSides[c]) {
        if (neighbours[c] <= settings.maxNeighbours)
          break;
        Merge(sides[index]);
      }
    std::vector<uint32_t> remap(corners.size(), NONE);
    std::vector<glm::vec2> kept;
    for (auto &polygon : polygons) {
      std::vector<uint32_t> merged;
      for (const auto corner : polygon) {
        const auto root = parent[corner];
        if (merged.empty() || merged.back() != root)
          merged.push_back(root);
      }
      while (merged.size() > 1 && merged.front() == merged.back())
        merged.pop_back();
      for (auto &corner : merged) {
        if (remap[corner] == NONE) {
          remap[corner] = static_cast<uint32_t>(kept.size());
          kept.push_back(corners[corner]);
        }
        corner = remap[corner];
      }
      polygon = std::move(merged);
    }
    corners = std::move(kept);
  }
} // namespace
IrregularGrid::IrregularGrid(std::vector<glm::vec2> corners, std::vector<std::vector<uint32_t>> polygons)
  : corners(std::move(corners)) {
  cells.resize(polygons.size());
  for (size_t i = 0; i < polygons.size(); ++i)
    cells[i].corners = std::move(polygons[i]);
  Build();
}
auto IrregularGrid::Voronoi(const GridSettings &settings) -> IrregularGrid {
  Random random(settings.seed, 0x6a09e667f3bcc909ull);
  const auto spacing = static_cast<double>(std::max(settings.spacing, 1e-3f));
  // A margin of sites beyond the rectangle, so the cells whose sites are inside it are closed on
  // every side by real neighbours rather than by the hull. Their cells are thrown away at the end.
  const auto margin = 2.5 * spacing;
  const glm::dvec2 low{-margin};
  const glm::dvec2 high = glm::dvec2(settings.size) + margin;
  // Points kept this far apart come out at about one per `spacing` squared, which is what makes the
  // grid's measured spacing come out near the one asked for.
  auto points = ScatterPoints(low, high, spacing * .82, random);
  for (auto round = 0; round < settings.relaxation; ++round) {
    const auto [triangles, count] = Triangulate(points);
    const auto fans = BuildFans(triangles, count);
    for (uint32_t s = 0; s < count; ++s) {
      if (!fans[s])
        continue;
      std::vector<glm::dvec2> polygon;
      for (const auto t : fans[s]->triangles)
        polygon.push_back(triangles[t].centre);
      if (const auto centroid = PolygonCentroid(polygon))
        points[s] = glm::clamp(*centroid, low, high);
    }
  }
  const auto [triangles, count] = Triangulate(points);
  const auto fans = BuildFans(triangles, count);
  std::vector<uint32_t> cellOf(count, NONE);
  std::vector<uint32_t> sites;
  for (uint32_t s = 0; s < count; ++s) {
    const auto &p = points[s];
    if (fans[s] && p.x >= .0 && p.y >= .0 && p.x < settings.size.x && p.y < settings.size.y) {
      cellOf[s] = static_cast<uint32_t>(sites.size());
      sites.push_back(s);
    }
  }
  std::vector<uint32_t> cornerOf(triangles.size(), NONE);
  std::vector<glm::vec2> corners;
  std::vector<std::vector<uint32_t>> polygons;
  polygons.reserve(sites.size());
  for (const auto s : sites) {
    std::vector<uint32_t> polygon;
    for (const auto t : fans[s]->triangles) {
      if (cornerOf[t] == NONE) {
        cornerOf[t] = static_cast<uint32_t>(corners.size());
        corners.emplace_back(triangles[t].centre);
      }
      polygon.push_back(cornerOf[t]);
    }
    polygons.push_back(std::move(polygon));
  }
  // Two triangles whose centres are the same point make a side of no length; it goes with the short
  // ones, but a corner listed twice in a row would make a polygon with a side from a point to itself.
  for (auto &polygon : polygons) {
    std::vector<uint32_t> distinct;
    for (const auto corner : polygon)
      if (distinct.empty() || glm::distance(corners[distinct.back()], corners[corner]) > 1e-6f)
        distinct.push_back(corner);
    while (distinct.size() > 1 && glm::distance(corners[distinct.front()], corners[distinct.back()]) <= 1e-6f)
      distinct.pop_back();
    polygon = std::move(distinct);
  }
  CollapseSides(corners, polygons, settings);
  IrregularGrid grid(std::move(corners), std::move(polygons));
  for (size_t i = 0; i < sites.size(); ++i)
    grid.cells[i].site = glm::vec2(points[sites[i]]);
  return grid;
}
auto IrregularGrid::Build() -> void {
  // Each directed side, by its two ends, and whose it is: a side from `p` to `q` is shared with the
  // cell that has one from `q` to `p`.
  std::unordered_map<uint64_t, std::pair<CellIndex, uint32_t>> directed;
  for (CellIndex c = 0; c < cells.size(); ++c) {
    const auto &list = cells[c].corners;
    for (uint32_t i = 0; i < list.size(); ++i)
      directed[static_cast<uint64_t>(list[i]) << 32u | list[(i + 1) % list.size()]] = {c, i};
  }
  auto totalArea = .0f;
  min = glm::vec2(std::numeric_limits<float>::max());
  max = glm::vec2(std::numeric_limits<float>::lowest());
  for (const auto &corner : corners) {
    min = glm::min(min, corner);
    max = glm::max(max, corner);
  }
  edges.clear();
  std::vector<uint32_t> cornerCount(corners.size(), 0u);
  for (CellIndex c = 0; c < cells.size(); ++c) {
    auto &cell = cells[c];
    const auto &list = cell.corners;
    const auto n = list.size();
    cell.across.assign(n, INVALID_CELL);
    cell.neighbours.clear();
    auto area = .0f;
    glm::vec2 sum{};
    for (uint32_t i = 0; i < n; ++i) {
      const auto from = list[i];
      const auto to = list[(i + 1) % n];
      if (const auto it = directed.find(static_cast<uint64_t>(to) << 32u | from); it != directed.end())
        cell.across[i] = it->second.first;
      if (cell.across[i] != INVALID_CELL)
        cell.neighbours.push_back(cell.across[i]);
      if (cell.across[i] == INVALID_CELL || c < cell.across[i])
        edges.push_back({from, to, c, cell.across[i]});
      const auto &a = corners[from];
      const auto &b = corners[to];
      const auto cross = a.x * b.y - b.x * a.y;
      area += cross;
      sum += (a + b) * cross;
      ++cornerCount[from];
    }
    cell.area = area * .5f;
    cell.centroid = std::abs(area) > 1e-9f ? sum / (3.f * area) : (n ? corners[list[0]] : glm::vec2{});
    cell.site = cell.centroid;
    totalArea += std::abs(cell.area);
  }
  cornerOffsets.assign(corners.size() + 1, 0u);
  for (size_t i = 0; i < corners.size(); ++i)
    cornerOffsets[i + 1] = cornerOffsets[i] + cornerCount[i];
  cornerCells.assign(cornerOffsets.back(), INVALID_CELL);
  std::vector<uint32_t> fill(cornerOffsets.begin(), cornerOffsets.end() - 1);
  for (CellIndex c = 0; c < cells.size(); ++c)
    for (const auto corner : cells[c].corners)
      cornerCells[fill[corner]++] = c;
  spacing = cells.empty() ? 1.f : std::sqrt(totalArea / static_cast<float>(cells.size()));
  // Buckets about two cells across: few enough cells in each that a lookup tests a handful, and
  // few enough buckets that a cell is listed in one to four of them.
  bucketSize = std::max(spacing * 2.f, 1e-3f);
  const auto extent = max - min;
  bucketCount = cells.empty() ? glm::ivec2(0) : glm::ivec2(std::max(1, static_cast<int>(std::ceil(extent.x / bucketSize))), std::max(1, static_cast<int>(std::ceil(extent.y / bucketSize))));
  const auto buckets = static_cast<size_t>(bucketCount.x) * bucketCount.y;
  std::vector<std::vector<CellIndex>> lists(buckets);
  for (CellIndex c = 0; c < cells.size(); ++c) {
    glm::vec2 low{std::numeric_limits<float>::max()};
    glm::vec2 high{std::numeric_limits<float>::lowest()};
    for (const auto corner : cells[c].corners) {
      low = glm::min(low, corners[corner]);
      high = glm::max(high, corners[corner]);
    }
    const auto a = BucketOf(low);
    const auto b = BucketOf(high);
    for (auto y = a.y; y <= b.y; ++y)
      for (auto x = a.x; x <= b.x; ++x)
        lists[static_cast<size_t>(y) * bucketCount.x + x].push_back(c);
  }
  bucketOffsets.assign(buckets + 1, 0u);
  bucketCells.clear();
  for (size_t i = 0; i < buckets; ++i) {
    bucketCells.insert(bucketCells.end(), lists[i].begin(), lists[i].end());
    bucketOffsets[i + 1] = static_cast<uint32_t>(bucketCells.size());
  }
}
auto IrregularGrid::BucketOf(const glm::vec2 &point) const -> glm::ivec2 {
  const auto relative = (point - min) / bucketSize;
  return {std::clamp(static_cast<int>(std::floor(relative.x)), 0, std::max(0, bucketCount.x - 1)), std::clamp(static_cast<int>(std::floor(relative.y)), 0, std::max(0, bucketCount.y - 1))};
}
auto IrregularGrid::CellCount() const -> size_t {
  return cells.size();
}
auto IrregularGrid::GetCell(const CellIndex index) const -> const GridCell & {
  return cells[index];
}
auto IrregularGrid::GetCells() const -> std::span<const GridCell> {
  return cells;
}
auto IrregularGrid::GetCorner(const uint32_t index) const -> glm::vec2 {
  return corners[index];
}
auto IrregularGrid::GetCorners() const -> std::span<const glm::vec2> {
  return corners;
}
auto IrregularGrid::GetCornerCells(const uint32_t corner) const -> std::span<const CellIndex> {
  return std::span(cornerCells).subspan(cornerOffsets[corner], cornerOffsets[corner + 1] - cornerOffsets[corner]);
}
auto IrregularGrid::GetEdges() const -> std::span<const GridEdge> {
  return edges;
}
auto IrregularGrid::GetSpacing() const -> float {
  return spacing;
}
auto IrregularGrid::GetMin() const -> glm::vec2 {
  return min;
}
auto IrregularGrid::GetMax() const -> glm::vec2 {
  return max;
}
auto IrregularGrid::AreNeighbours(const CellIndex a, const CellIndex b) const -> bool {
  return a < cells.size() && std::ranges::find(cells[a].neighbours, b) != cells[a].neighbours.end();
}
auto IrregularGrid::IsRim(const CellIndex cell) const -> bool {
  return std::ranges::find(cells[cell].across, INVALID_CELL) != cells[cell].across.end();
}
auto IrregularGrid::SharedSide(const CellIndex a, const CellIndex b) const -> std::optional<std::pair<glm::vec2, glm::vec2>> {
  if (a >= cells.size())
    return std::nullopt;
  const auto &cell = cells[a];
  for (size_t i = 0; i < cell.across.size(); ++i)
    if (cell.across[i] == b)
      return std::pair{corners[cell.corners[i]], corners[cell.corners[(i + 1) % cell.corners.size()]]};
  return std::nullopt;
}
auto IrregularGrid::Contains(const CellIndex index, const glm::vec2 &point) const -> bool {
  // Crossings of a ray to the right, which holds for any simple polygon and so does not rely on the
  // cell being exactly convex after its short sides were shrunk.
  const auto &list = cells[index].corners;
  auto inside = false;
  for (size_t i = 0, j = list.size() - 1; i < list.size(); j = i++) {
    const auto &a = corners[list[i]];
    const auto &b = corners[list[j]];
    if ((a.y > point.y) != (b.y > point.y) && point.x < (b.x - a.x) * (point.y - a.y) / (b.y - a.y) + a.x)
      inside = !inside;
  }
  return inside;
}
auto IrregularGrid::Locate(const glm::vec2 &point) const -> CellIndex {
  if (cells.empty() || point.x < min.x || point.y < min.y || point.x > max.x || point.y > max.y)
    return INVALID_CELL;
  const auto bucket = BucketOf(point);
  const auto index = static_cast<size_t>(bucket.y) * bucketCount.x + bucket.x;
  for (auto i = bucketOffsets[index]; i < bucketOffsets[index + 1]; ++i)
    if (Contains(bucketCells[i], point))
      return bucketCells[i];
  return INVALID_CELL;
}
auto IrregularGrid::Nearest(const glm::vec2 &point) const -> CellIndex {
  if (const auto cell = Locate(point); cell != INVALID_CELL)
    return cell;
  auto best = INVALID_CELL;
  auto bestDistance = std::numeric_limits<float>::max();
  for (CellIndex c = 0; c < cells.size(); ++c) {
    const auto d = cells[c].centroid - point;
    if (const auto distance = d.x * d.x + d.y * d.y; distance < bestDistance) {
      bestDistance = distance;
      best = c;
    }
  }
  return best;
}
auto IrregularGrid::Within(const CellIndex start, const int steps) const -> std::vector<CellIndex> {
  std::vector<CellIndex> found;
  if (start >= cells.size())
    return found;
  std::unordered_map<CellIndex, int> seen{{start, 0}};
  found.push_back(start);
  for (size_t i = 0; i < found.size(); ++i) {
    const auto depth = seen[found[i]];
    if (depth >= steps)
      continue;
    for (const auto next : cells[found[i]].neighbours)
      if (seen.try_emplace(next, depth + 1).second)
        found.push_back(next);
  }
  return found;
}
auto IrregularGrid::Footprint(const glm::vec2 &anchor, const float angle, const std::span<const glm::vec2> shape) const -> std::vector<CellIndex> {
  std::vector<CellIndex> taken;
  if (shape.empty())
    return taken;
  const auto c = std::cos(angle);
  const auto s = std::sin(angle);
  const auto Place = [&](const glm::vec2 &offset) {
    const auto local = (offset - shape[0]) * spacing;
    return anchor + glm::vec2(local.x * c - local.y * s, local.x * s + local.y * c);
  };
  const auto first = Locate(anchor);
  if (first == INVALID_CELL)
    return taken;
  taken.push_back(first);
  for (size_t i = 1; i < shape.size(); ++i) {
    const auto target = Place(shape[i]);
    auto best = INVALID_CELL;
    auto bestDistance = std::numeric_limits<float>::max();
    for (const auto cell : taken)
      for (const auto next : cells[cell].neighbours) {
        if (std::ranges::find(taken, next) != taken.end())
          continue;
        const auto d = cells[next].centroid - target;
        // Ties settled by index, so the answer does not depend on which taken cell was looked at first.
        if (const auto distance = d.x * d.x + d.y * d.y; distance < bestDistance || (distance == bestDistance && next < best)) {
          bestDistance = distance;
          best = next;
        }
      }
    if (best == INVALID_CELL)
      return {};
    taken.push_back(best);
  }
  return taken;
}
auto IrregularGrid::Middle(const std::span<const CellIndex> list) const -> glm::vec2 {
  glm::vec2 sum{};
  auto area = .0f;
  for (const auto cell : list) {
    sum += cells[cell].centroid * cells[cell].area;
    area += cells[cell].area;
  }
  return area > .0f ? sum / area : glm::vec2{};
}
auto IrregularGrid::NeighbourRange() const -> std::pair<uint32_t, uint32_t> {
  auto low = std::numeric_limits<uint32_t>::max();
  auto high = 0u;
  for (CellIndex c = 0; c < cells.size(); ++c) {
    if (IsRim(c))
      continue;
    const auto count = static_cast<uint32_t>(cells[c].neighbours.size());
    low = std::min(low, count);
    high = std::max(high, count);
  }
  return {low == std::numeric_limits<uint32_t>::max() ? 0u : low, high};
}
} // namespace kuki
