#include <algorithm>
#include <array>
#include <gtest/gtest.h>
#include <ranges>
#include <set>
#include <terrain.hpp>
using namespace frontier;
using namespace kuki;
namespace {
auto Small(const uint64_t seed) -> TerrainMap {
  return GenerateMap({.seed = seed, .size = {40.f, 30.f}});
}
/// Which terrains may meet, as the map's rules have them, both ways round.
auto MayMeet(const Terrain a, const Terrain b) -> bool {
  if (a == b)
    return true;
  const std::set<std::pair<Terrain, Terrain>> pairs{
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
  return pairs.contains({a, b}) || pairs.contains({b, a});
}
} // namespace
TEST(FrontierTerrain, KeepsToItsRulesEverywhere) {
  for (const auto seed : {1ull, 2ull, 3ull}) {
    const auto map = Small(seed);
    for (CellIndex c = 0; c < map.CellCount(); ++c) {
      if (map.grid.IsRim(c))
        EXPECT_EQ(map.terrain[c], Terrain::Deep) << "the rim is sea";
      for (const auto n : map.grid.GetCell(c).neighbours)
        EXPECT_TRUE(MayMeet(map.terrain[c], map.terrain[n])) << "seed " << seed << ": " << Info(map.terrain[c]).name << " beside " << Info(map.terrain[n]).name;
    }
  }
}
TEST(FrontierTerrain, IsAnIslandWithEveryKindOfGround) {
  const auto map = Small(4);
  std::array<int, TERRAIN_COUNT> counts{};
  for (const auto terrain : map.terrain)
    ++counts[static_cast<size_t>(terrain)];
  const auto cells = static_cast<float>(map.CellCount());
  const auto water = static_cast<float>(counts[0] + counts[1]) / cells;
  EXPECT_GT(water, .3f);
  EXPECT_LT(water, .65f);
  // Mountains, but a range and not the whole island.
  EXPECT_LT(static_cast<float>(counts[static_cast<size_t>(Terrain::Rock)]) / cells, .2f);
  for (const auto t : {Terrain::Sand, Terrain::Grass, Terrain::Forest, Terrain::Hills})
    EXPECT_GT(counts[static_cast<size_t>(t)], 0) << Info(t).name;
}
TEST(FrontierTerrain, IsTheSameForTheSameSeed) {
  const auto a = Small(9);
  const auto b = Small(9);
  ASSERT_EQ(a.CellCount(), b.CellCount());
  EXPECT_EQ(a.terrain, b.terrain);
  EXPECT_EQ(a.resource, b.resource);
  EXPECT_EQ(a.height, b.height);
}
TEST(FrontierTerrain, LaysResourcesOverWholeJoinedCells) {
  const auto map = Small(5);
  ASSERT_FALSE(map.sites.empty());
  for (size_t s = 0; s < map.sites.size(); ++s) {
    const auto &site = map.sites[s];
    ASSERT_FALSE(site.cells.empty());
    for (const auto cell : site.cells)
      EXPECT_EQ(map.resource[cell], static_cast<int>(s));
    // A deposit of several cells is one piece.
    for (size_t i = 1; i < site.cells.size(); ++i)
      EXPECT_TRUE(std::any_of(site.cells.begin(), site.cells.begin() + static_cast<std::ptrdiff_t>(i), [&](const CellIndex other) { return map.grid.AreNeighbours(site.cells[i], other); }));
    if (site.kind == Resource::Fish)
      EXPECT_EQ(map.terrain[site.cells[0]], Terrain::Shallows);
    if (site.kind == Resource::Timber)
      EXPECT_EQ(map.terrain[site.cells[0]], Terrain::Forest);
  }
}
TEST(FrontierTerrain, MovementFollowsTheGroundAndTheBuildings) {
  auto map = Small(6);
  for (CellIndex c = 0; c < map.CellCount(); ++c) {
    if (IsWater(map.terrain[c]) || IsPeaked(map.terrain[c]))
      EXPECT_TRUE(map.nav.IsBlocked(c));
    if (map.nav.IsBlocked(c))
      EXPECT_FALSE(map.IsFree(c) && map.MoveCost(c) > .0f);
  }
  // A building takes its cell from the movement layer, and gives it back when it goes.
  const auto free = std::ranges::find_if(std::views::iota(CellIndex{0}, static_cast<CellIndex>(map.CellCount())), [&](const CellIndex c) { return map.IsFree(c); });
  ASSERT_NE(free, std::views::iota(CellIndex{0}, static_cast<CellIndex>(map.CellCount())).end());
  const auto cell = *free;
  EXPECT_FALSE(map.nav.IsBlocked(cell));
  map.occupant[cell] = 0;
  map.RefreshNavigation(cell);
  EXPECT_TRUE(map.nav.IsBlocked(cell));
  EXPECT_FALSE(map.IsFree(cell));
  map.occupant[cell] = -1;
  map.RefreshNavigation(cell);
  EXPECT_FALSE(map.nav.IsBlocked(cell));
}
TEST(FrontierTerrain, GroundIsFlatOnATerraceAndRisesToAPeak) {
  const auto map = Small(7);
  for (CellIndex c = 0; c < map.CellCount(); ++c) {
    const auto middle = map.grid.GetCell(c).centroid;
    if (IsPeaked(map.terrain[c]))
      EXPECT_NEAR(map.GroundAt(middle), map.height[c] + map.peak[c], 1e-3f);
    else
      EXPECT_FLOAT_EQ(map.GroundAt(middle), map.height[c]);
  }
}
