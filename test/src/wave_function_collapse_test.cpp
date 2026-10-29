#include <gtest/gtest.h>
#include <irregular_grid.hpp>
#include <wave_function_collapse.hpp>
using namespace kuki;
namespace {
/// Tiles in a chain, each allowed next to itself and the ones either side: the shape of a terrain
/// that goes from the sea to the mountains by way of everything between.
constexpr TileIndex CHAIN = 6;
auto ChainRules() -> WaveRules {
  WaveRules rules(CHAIN);
  for (TileIndex t = 0; t < CHAIN; ++t) {
    rules.AllowSelf(t);
    if (t + 1 < CHAIN)
      rules.Allow(t, t + 1);
  }
  return rules;
}
auto MakeGrid() -> IrregularGrid {
  GridSettings settings;
  settings.size = {20.f, 20.f};
  settings.seed = 11;
  return IrregularGrid::Voronoi(settings);
}
auto Only(const TileIndex tile) -> TileSet {
  TileSet set;
  set.set(tile);
  return set;
}
} // namespace
TEST(WaveFunctionCollapse, FillsAnIrregularGridWithinTheRules) {
  const auto grid = MakeGrid();
  WaveFunctionCollapse wave(WaveGraph::FromGrid(grid), ChainRules());
  for (CellIndex c = 0; c < grid.CellCount(); ++c)
    if (grid.IsRim(c))
      wave.Restrict(c, Only(0));
  ASSERT_TRUE(wave.Run({.seed = 5}));
  for (CellIndex c = 0; c < grid.CellCount(); ++c) {
    const auto tile = wave.Get(c);
    ASSERT_NE(tile, INVALID_TILE);
    if (grid.IsRim(c))
      EXPECT_EQ(tile, 0u);
    for (const auto n : grid.GetCell(c).neighbours) {
      const auto other = wave.Get(n);
      EXPECT_LE(tile > other ? tile - other : other - tile, 1u) << "cells " << c << " and " << n;
    }
  }
}
TEST(WaveFunctionCollapse, IsTheSameForTheSameSeed) {
  const auto grid = MakeGrid();
  WaveFunctionCollapse a(WaveGraph::FromGrid(grid), ChainRules());
  WaveFunctionCollapse b(WaveGraph::FromGrid(grid), ChainRules());
  WaveFunctionCollapse c(WaveGraph::FromGrid(grid), ChainRules());
  ASSERT_TRUE(a.Run({.seed = 9}));
  ASSERT_TRUE(b.Run({.seed = 9}));
  ASSERT_TRUE(c.Run({.seed = 10}));
  auto differences = 0;
  for (uint32_t n = 0; n < a.NodeCount(); ++n) {
    EXPECT_EQ(a.Get(n), b.Get(n));
    differences += a.Get(n) != c.Get(n);
  }
  EXPECT_GT(differences, 0);
}
TEST(WaveFunctionCollapse, BiasShapesTheResult) {
  const auto grid = MakeGrid();
  WaveFunctionCollapse wave(WaveGraph::FromGrid(grid), ChainRules());
  // The left half wants the sea and the right half the mountains.
  for (CellIndex c = 0; c < grid.CellCount(); ++c) {
    const auto left = grid.GetCell(c).centroid.x < 10.f;
    wave.Bias(c, left ? 0 : CHAIN - 1, 40.f);
  }
  ASSERT_TRUE(wave.Run({.seed = 2}));
  auto leftSum = 0.;
  auto rightSum = 0.;
  auto leftCount = 0;
  auto rightCount = 0;
  for (CellIndex c = 0; c < grid.CellCount(); ++c) {
    const auto tile = static_cast<double>(wave.Get(c));
    if (grid.GetCell(c).centroid.x < 10.f) {
      leftSum += tile;
      ++leftCount;
    } else {
      rightSum += tile;
      ++rightCount;
    }
  }
  EXPECT_LT(leftSum / leftCount + 1.5, rightSum / rightCount);
}
TEST(WaveFunctionCollapse, SaysSoWhenNoTileFits) {
  // A wheel: a hub with five rim nodes in a ring. With the chain rules and the rim fixed to
  // alternate far ends, no hub tile fits, and the solver has to say so.
  WaveGraph graph;
  const std::vector<WaveGraph::Side> hub{{1}, {2}, {3}, {4}, {5}};
  graph.AddNode(hub);
  for (uint32_t i = 1; i <= 5; ++i) {
    const std::vector<WaveGraph::Side> sides{{0}, {i == 5 ? 1u : i + 1}, {i == 1 ? 5u : i - 1}};
    graph.AddNode(sides);
  }
  WaveFunctionCollapse impossible(graph, ChainRules());
  impossible.Restrict(1, Only(0));
  impossible.Restrict(3, Only(4));
  EXPECT_FALSE(impossible.Run({.seed = 1, .attempts = 2}));
  // With room to agree it finds the one way: everything between the two ends.
  WaveFunctionCollapse possible(graph, ChainRules());
  possible.Restrict(1, Only(1));
  possible.Restrict(3, Only(3));
  ASSERT_TRUE(possible.Run({.seed = 1}));
  EXPECT_EQ(possible.Get(0), 2u);
}
TEST(WaveFunctionCollapse, ReadsKindsOfSideOneWay) {
  // A line of nodes whose sides point uphill one way and downhill the other: a tile may only be
  // followed uphill by itself or the next one up.
  constexpr uint32_t UP = 0;
  constexpr uint32_t DOWN = 1;
  WaveRules rules(4, 2);
  for (TileIndex t = 0; t < 4; ++t) {
    rules.Allow(t, t, UP, DOWN);
    if (t + 1 < 4)
      rules.Allow(t, t + 1, UP, DOWN);
  }
  WaveGraph graph;
  constexpr uint32_t LENGTH = 12;
  for (uint32_t i = 0; i < LENGTH; ++i) {
    std::vector<WaveGraph::Side> sides;
    if (i + 1 < LENGTH)
      sides.push_back({i + 1, UP});
    if (i > 0)
      sides.push_back({i - 1, DOWN});
    graph.AddNode(sides);
  }
  WaveFunctionCollapse wave(graph, rules);
  ASSERT_TRUE(wave.Run({.seed = 4}));
  for (uint32_t i = 0; i + 1 < LENGTH; ++i) {
    EXPECT_GE(wave.Get(i + 1), wave.Get(i));
    EXPECT_LE(wave.Get(i + 1), wave.Get(i) + 1);
  }
}
