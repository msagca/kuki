#include <algorithm>
#include <chrono>
#include <cmath>
#include <glm/geometric.hpp>
#include <limits>
#include <noise.hpp>
#include <numeric>
#include <random.hpp>
#include <spdlog/spdlog.h>
#include <terrain.hpp>
#include <wave_function_collapse.hpp>
using namespace kuki;
namespace frontier {
namespace {
  auto Index(const Terrain terrain) -> TileIndex {
    return static_cast<TileIndex>(terrain);
  }
  /// @brief Which tiles may meet. Each may meet itself, and the pairs here, both ways round.
  ///
  /// A chain from the deep sea to the snow with a few short cuts that a coast has in life: rock
  /// straight out of the shallows is a sea cliff, and sand runs up to the woods and the hills as a
  /// dune does. Without that last, a cell with seven neighbours between the sea and the hills had
  /// only rock to be, and every one of those grew a ring of hills of its own.
  auto Rules() -> WaveRules {
    WaveRules rules(TERRAIN_COUNT);
    for (TileIndex t = 0; t < TERRAIN_COUNT; ++t)
      rules.AllowSelf(t);
    const std::pair<Terrain, Terrain> pairs[]{
      {Terrain::Deep, Terrain::Shallows},
      {Terrain::Shallows, Terrain::Sand},
      {Terrain::Shallows, Terrain::Rock},
      {Terrain::Sand, Terrain::Grass},
      {Terrain::Sand, Terrain::Meadow},
      {Terrain::Sand, Terrain::Forest},
      {Terrain::Sand, Terrain::Hills},
      {Terrain::Grass, Terrain::Meadow},
      {Terrain::Grass, Terrain::Forest},
      {Terrain::Grass, Terrain::Hills},
      {Terrain::Meadow, Terrain::Forest},
      {Terrain::Meadow, Terrain::Hills},
      {Terrain::Forest, Terrain::Hills},
      {Terrain::Hills, Terrain::Rock},
      {Terrain::Rock, Terrain::Snow},
    };
    for (const auto &[a, b] : pairs)
      rules.Allow(Index(a), Index(b));
    return rules;
  }
  /// @brief Each cell's place in a list of values sorted, from nought for the least to one for the most.
  auto Rank(const std::vector<float> &values, const std::vector<uint32_t> &cells) -> std::vector<float> {
    auto order = cells;
    std::ranges::sort(order, [&](const uint32_t a, const uint32_t b) { return values[a] < values[b]; });
    std::vector<float> rank(values.size(), .0f);
    for (size_t i = 0; i < order.size(); ++i)
      rank[order[i]] = order.size() > 1 ? static_cast<float>(i) / static_cast<float>(order.size() - 1) : .0f;
    return rank;
  }
  /// @brief How high the land wants to be at each cell, from nought to one.
  ///
  /// Two fields, each taken as a rank rather than a value so that every seed has about the same share
  /// of each thing. The first is an island -- high in the middle, sinking to the rim, with noise for
  /// the bays and capes -- and says only which cells are sea: the lowest `seaShare` of it. The second
  /// says how high the land is, and is mostly ridges, so the mountains are ranges running across the
  /// island where the ridges are rather than one heap at its middle, which is where the first field
  /// alone put them. The sea takes the bottom of the scale and the land the rest, each in its order.
  auto Elevation(const IrregularGrid &grid, const glm::vec2 &size, const uint32_t seed) -> std::vector<float> {
    constexpr float SEA_SHARE = .42f;
    constexpr float SEA_TOP = .36f;
    const auto count = grid.CellCount();
    std::vector<float> coast(count);
    std::vector<float> relief(count);
    const auto centre = size * .5f;
    for (CellIndex c = 0; c < count; ++c) {
      const auto p = grid.GetCell(c).centroid;
      const auto d = glm::length((p - centre) / centre);
      const auto island = 1.f - std::clamp((d - .2f) / .85f, .0f, 1.f);
      // Smooth at the scale of a cell. A field whose finest octave turns over every couple of cells
      // asks for a meadow beside a mountain, which the rules cannot give, and the cells settled
      // first -- the ones the bias is surest of, the peaks -- win the argument: a field of four
      // octaves came out a third rock where it asked for an eighth.
      coast[c] = island + FractalNoise(p * .045f, 3, seed) * .5f;
      relief[c] = RidgedNoise(p * .035f + 17.f, 2, seed + 101u) * .55f + FractalNoise(p * .025f, 2, seed + 55u) * .15f;
    }
    // The land rises from its shore, as well as along the ridges: a shore cell may only be sand or
    // rock beside the shallows, and a shore whose relief asked for a meadow came out rock about as
    // often as sand -- each rock with a ring of hills round it inland, and the hills with theirs.
    for (CellIndex c = 0; c < count; ++c)
      relief[c] += coast[c] * .7f;
    std::vector<uint32_t> all(count);
    std::iota(all.begin(), all.end(), 0u);
    const auto coastRank = Rank(coast, all);
    std::vector<uint32_t> sea;
    std::vector<uint32_t> land;
    for (CellIndex c = 0; c < count; ++c)
      (coastRank[c] < SEA_SHARE ? sea : land).push_back(c);
    const auto seaRank = Rank(coast, sea);
    const auto landRank = Rank(relief, land);
    std::vector<float> elevation(count);
    for (const auto c : sea)
      elevation[c] = seaRank[c] * SEA_TOP;
    for (const auto c : land)
      elevation[c] = SEA_TOP + landRank[c] * (1.f - SEA_TOP);
    return elevation;
  }
  auto ShapeOf(const int cells) -> std::vector<glm::vec2> {
    if (cells <= 1)
      return {{.0f, .0f}};
    if (cells == 2)
      return {{.0f, .0f}, {1.f, .0f}};
    return {{.0f, .0f}, {1.f, .0f}, {.5f, .87f}};
  }
} // namespace
auto Info(const Terrain terrain) -> const TerrainInfo & {
  return TERRAIN[static_cast<size_t>(terrain)];
}
auto IsWater(const Terrain terrain) -> bool {
  return terrain == Terrain::Deep || terrain == Terrain::Shallows;
}
auto IsPeaked(const Terrain terrain) -> bool {
  return terrain == Terrain::Rock || terrain == Terrain::Snow;
}
auto TerrainMap::CellCount() const -> size_t {
  return grid.CellCount();
}
auto TerrainMap::SideHeights(const CellIndex cell, const size_t side) const -> std::pair<float, float> {
  if (!IsPeaked(terrain[cell]))
    return {height[cell], height[cell]};
  const auto &corners = grid.GetCell(cell).corners;
  return {cornerHeight[corners[side]], cornerHeight[corners[(side + 1) % corners.size()]]};
}
auto TerrainMap::GroundAt(const glm::vec2 &point) const -> float {
  const auto cell = grid.Locate(point);
  if (cell == INVALID_CELL)
    return -std::numeric_limits<float>::infinity();
  if (!IsPeaked(terrain[cell]))
    return height[cell];
  // Which of the fan's triangles the point is in, and where in it.
  const auto &shape = grid.GetCell(cell);
  const auto middle = shape.centroid;
  for (size_t i = 0; i < shape.corners.size(); ++i) {
    const auto a = grid.GetCorner(shape.corners[i]);
    const auto b = grid.GetCorner(shape.corners[(i + 1) % shape.corners.size()]);
    const auto v0 = a - middle;
    const auto v1 = b - middle;
    const auto v2 = point - middle;
    const auto denominator = v0.x * v1.y - v1.x * v0.y;
    if (std::abs(denominator) < 1e-9f)
      continue;
    const auto u = (v2.x * v1.y - v1.x * v2.y) / denominator;
    const auto v = (v0.x * v2.y - v2.x * v0.y) / denominator;
    if (u < -1e-4f || v < -1e-4f || u + v > 1.f + 1e-4f)
      continue;
    const auto [ha, hb] = SideHeights(cell, i);
    return (height[cell] + peak[cell]) * (1.f - u - v) + ha * u + hb * v;
  }
  return height[cell] + peak[cell] * .5f;
}
auto TerrainMap::IsFree(const CellIndex cell) const -> bool {
  return cell < terrain.size() && Info(terrain[cell]).buildable && resource[cell] < 0 && occupant[cell] < 0 && !grid.IsRim(cell);
}
auto TerrainMap::MoveCost(const CellIndex cell) const -> float {
  if (occupant[cell] >= 0)
    return .0f;
  auto cost = Info(terrain[cell]).cost;
  if (const auto site = resource[cell]; site >= 0)
    switch (sites[static_cast<size_t>(site)].kind) {
    case Resource::Stone:
    case Resource::Ore:
      return .0f;
    case Resource::Berries:
      cost *= 1.25f;
      break;
    default:
      break;
    }
  return cost;
}
auto TerrainMap::RefreshNavigation(const CellIndex cell) -> void {
  const auto cost = MoveCost(cell);
  nav.SetBlocked(cell, cost <= .0f);
  if (cost > .0f)
    nav.SetCost(cell, cost);
}
auto TerrainMap::IsCoast(const CellIndex cell) const -> bool {
  if (IsWater(terrain[cell]))
    return false;
  return std::ranges::any_of(grid.GetCell(cell).neighbours, [&](const CellIndex n) { return IsWater(terrain[n]); });
}
auto GenerateMap(const MapSettings &settings) -> TerrainMap {
  const auto started = std::chrono::steady_clock::now();
  TerrainMap map;
  map.seed = settings.seed;
  const auto seed = static_cast<uint32_t>(settings.seed * 2654435761ull);
  map.grid = IrregularGrid::Voronoi({.size = settings.size, .spacing = settings.spacing, .relaxation = 2, .seed = settings.seed});
  const auto &grid = map.grid;
  const auto count = grid.CellCount();
  const auto elevation = Elevation(grid, settings.size, seed);
  // The terrain: every tile weighted at each cell by how near the cell's elevation is to the one the
  // tile likes, and the forest and the meadow by how wet the ground is. The rules then keep it
  // coherent cell to cell, and the bias gives it its shape map-wide.
  WaveFunctionCollapse wave(WaveGraph::FromGrid(grid), Rules());
  TileSet sea;
  sea.set(Index(Terrain::Deep));
  for (CellIndex c = 0; c < count; ++c) {
    const auto p = grid.GetCell(c).centroid;
    const auto e = elevation[c];
    const auto wet = .5f + .5f * FractalNoise(p * .07f, 3, seed + 77u);
    if (grid.IsRim(c) || e < .1f)
      wave.Restrict(c, sea);
    for (TileIndex t = 0; t < TERRAIN_COUNT; ++t) {
      const auto d = (e - TERRAIN[t].elevation) / settings.bandWidth;
      auto factor = settings.bias * std::exp(-d * d) + .04f;
      if (t == Index(Terrain::Forest))
        factor *= .2f + 1.8f * wet * wet;
      else if (t == Index(Terrain::Meadow))
        factor *= .3f + 1.4f * (1.f - wet);
      wave.Bias(c, t, factor);
    }
  }
  const auto solved = wave.Run({.seed = settings.seed});
  map.repairs = wave.GetRepairs();
  map.attempts = wave.GetAttempts();
  if (!solved)
    spdlog::warn("[Frontier] The terrain did not settle in {} attempts; unsettled cells are left as sea", map.attempts);
  map.terrain.resize(count);
  for (CellIndex c = 0; c < count; ++c) {
    const auto tile = wave.Get(c);
    map.terrain[c] = tile == INVALID_TILE ? Terrain::Deep : static_cast<Terrain>(tile);
  }
  // The ground. A terrace per cell, a few hundredths off its neighbours' so the cells can be told
  // apart where the ground is level; the hills higher where the field is; and a peak on every rock
  // and snow cell, as tall as the ridges are there.
  map.height.resize(count);
  map.peak.assign(count, .0f);
  for (CellIndex c = 0; c < count; ++c) {
    const auto p = grid.GetCell(c).centroid;
    const auto terrain = map.terrain[c];
    const auto jitter = (static_cast<float>(HashPoint(static_cast<int>(c), 3, seed) & 0xffffu) / 65535.f - .5f) * .07f;
    auto h = Info(terrain).height;
    if (!IsWater(terrain))
      h += jitter;
    if (terrain == Terrain::Hills)
      h += .25f * (.5f + .5f * FractalNoise(p * .12f, 2, seed + 5u));
    map.height[c] = h;
    const auto ridge = RidgedNoise(p * .09f, 3, seed + 31u);
    if (terrain == Terrain::Rock)
      map.peak[c] = .5f + 1.f * ridge;
    else if (terrain == Terrain::Snow)
      map.peak[c] = 1.4f + 1.2f * ridge;
  }
  // A peak's corners meet whatever is round it: the highest terrace there if any, so a mountain
  // comes down to the hills without a step, or else the peaks round it most of the way up, so the
  // peaks of a range join in ridges rather than standing as a field of separate spikes.
  map.cornerHeight.resize(grid.GetCorners().size());
  for (uint32_t corner = 0; corner < map.cornerHeight.size(); ++corner) {
    auto terrace = -std::numeric_limits<float>::infinity();
    auto sum = .0f;
    auto peaks = 0;
    for (const auto cell : grid.GetCornerCells(corner))
      if (IsPeaked(map.terrain[cell])) {
        sum += map.height[cell] + map.peak[cell] * .7f;
        ++peaks;
      } else
        terrace = std::max(terrace, map.height[cell]);
    map.cornerHeight[corner] = std::isfinite(terrace) ? terrace : (peaks ? sum / static_cast<float>(peaks) : .0f);
  }
  // The resources: a site on every forest cell, deposits of stone in the hills two or three cells
  // across, ore where the hills meet the rock, fish off the shore and berries in the meadows.
  map.resource.assign(count, -1);
  map.occupant.assign(count, -1);
  Random random(settings.seed, 0x1f83d9abfb41bd6bull);
  const auto AddSite = [&](const Resource kind, const std::vector<CellIndex> &cells, const int amount) {
    ResourceSite site{.kind = kind, .cells = cells, .middle = grid.Middle(cells), .angle = random.Range(.0f, 6.2831853f), .amount = amount};
    if (cells.size() > 1) {
      const auto along = grid.GetCell(cells[1]).centroid - grid.GetCell(cells[0]).centroid;
      site.angle = std::atan2(along.y, along.x);
    }
    for (const auto cell : cells)
      map.resource[cell] = static_cast<int>(map.sites.size());
    map.sites.push_back(std::move(site));
  };
  const auto Neighbouring = [&](const CellIndex cell, const Terrain terrain) {
    return std::ranges::any_of(grid.GetCell(cell).neighbours, [&](const CellIndex n) { return map.terrain[n] == terrain; });
  };
  const auto Fits = [&](const std::vector<CellIndex> &cells, std::initializer_list<Terrain> allowed) {
    return !cells.empty() && std::ranges::all_of(cells, [&](const CellIndex cell) {
      return map.resource[cell] < 0 && !grid.IsRim(cell) && std::ranges::find(allowed, map.terrain[cell]) != allowed.end();
    });
  };
  for (CellIndex c = 0; c < count; ++c)
    if (map.terrain[c] == Terrain::Forest)
      AddSite(Resource::Timber, {c}, 120 + static_cast<int>(random.Below(120)));
  std::vector<CellIndex> order(count);
  std::iota(order.begin(), order.end(), 0u);
  for (size_t i = order.size(); i > 1; --i)
    std::swap(order[i - 1], order[random.Below(static_cast<uint32_t>(i))]);
  for (const auto c : order) {
    const auto terrain = map.terrain[c];
    if (map.resource[c] >= 0 || grid.IsRim(c))
      continue;
    if (terrain == Terrain::Hills && Neighbouring(c, Terrain::Rock) && random.Chance(.1f)) {
      const auto shape = ShapeOf(random.Chance(.5f) ? 2 : 1);
      if (const auto cells = grid.Footprint(grid.GetCell(c).centroid, random.Range(.0f, 6.2831853f), shape); Fits(cells, {Terrain::Hills}))
        AddSite(Resource::Ore, cells, 300 + static_cast<int>(random.Below(300)));
    } else if (terrain == Terrain::Hills && random.Chance(.16f)) {
      const auto shape = ShapeOf(random.Chance(.5f) ? 3 : 2);
      if (const auto cells = grid.Footprint(grid.GetCell(c).centroid, random.Range(.0f, 6.2831853f), shape); Fits(cells, {Terrain::Hills, Terrain::Grass}))
        AddSite(Resource::Stone, cells, 400 + static_cast<int>(random.Below(400)));
    } else if (terrain == Terrain::Shallows && std::ranges::any_of(grid.GetCell(c).neighbours, [&](const CellIndex n) { return !IsWater(map.terrain[n]); }) && random.Chance(.12f)) {
      // Not beside another shoal: two together read as one.
      if (std::ranges::none_of(grid.GetCell(c).neighbours, [&](const CellIndex n) { return map.resource[n] >= 0; }))
        AddSite(Resource::Fish, {c}, 200 + static_cast<int>(random.Below(200)));
    } else if (terrain == Terrain::Meadow && random.Chance(.1f))
      AddSite(Resource::Berries, {c}, 80 + static_cast<int>(random.Below(80)));
  }
  map.nav = NavMesh::FromGrid(grid);
  for (CellIndex c = 0; c < count; ++c)
    map.RefreshNavigation(c);
  map.elapsed = std::chrono::duration<float>(std::chrono::steady_clock::now() - started).count();
  return map;
}
} // namespace frontier
