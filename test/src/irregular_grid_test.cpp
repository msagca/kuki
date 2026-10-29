#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <gtest/gtest.h>
#include <irregular_grid.hpp>
#include <set>
#include <vector>
using namespace kuki;
namespace {
auto MakeGrid(const uint64_t seed = 7) -> IrregularGrid {
  GridSettings settings;
  settings.size = {24.f, 18.f};
  settings.spacing = 1.f;
  settings.seed = seed;
  return IrregularGrid::Voronoi(settings);
}
} // namespace
TEST(IrregularGrid, GrowsAboutOneCellPerUnitOfArea) {
  const auto grid = MakeGrid();
  // The rectangle is 432 square units; the ragged rim takes a few cells either way.
  EXPECT_GT(grid.CellCount(), 330u);
  EXPECT_LT(grid.CellCount(), 540u);
  EXPECT_NEAR(grid.GetSpacing(), 1.f, .2f);
}
TEST(IrregularGrid, KeepsNeighbourCountsInRange) {
  const auto grid = MakeGrid();
  const auto [low, high] = grid.NeighbourRange();
  EXPECT_GE(low, 4u);
  // The upper bound gives way only where keeping it would take a neighbour below four, which a
  // relaxed grid all but never asks for.
  EXPECT_LE(high, 9u);
  auto inRange = 0;
  auto interior = 0;
  std::set<size_t> counts;
  for (CellIndex c = 0; c < grid.CellCount(); ++c) {
    if (grid.IsRim(c))
      continue;
    ++interior;
    const auto n = grid.GetCell(c).neighbours.size();
    counts.insert(n);
    inRange += n >= 4 && n <= 8;
  }
  EXPECT_GE(inRange, interior - 2);
  // The point of the grid: the counts are not all the same.
  EXPECT_GE(counts.size(), 3u);
}
TEST(IrregularGrid, NeighboursAreMutualAndShareASide) {
  const auto grid = MakeGrid();
  for (CellIndex c = 0; c < grid.CellCount(); ++c) {
    const auto &cell = grid.GetCell(c);
    ASSERT_EQ(cell.corners.size(), cell.across.size());
    for (const auto n : cell.neighbours) {
      EXPECT_TRUE(grid.AreNeighbours(n, c)) << c << " sees " << n << " but not the other way round";
      const auto side = grid.SharedSide(c, n);
      ASSERT_TRUE(side.has_value());
      const auto back = grid.SharedSide(n, c);
      ASSERT_TRUE(back.has_value());
      // The same side seen from the other cell runs the other way.
      EXPECT_EQ(side->first, back->second);
      EXPECT_EQ(side->second, back->first);
    }
  }
}
TEST(IrregularGrid, CellsAreCounterClockwiseAndConvex) {
  const auto grid = MakeGrid();
  for (CellIndex c = 0; c < grid.CellCount(); ++c) {
    const auto &cell = grid.GetCell(c);
    EXPECT_GT(cell.area, .0f);
    const auto n = cell.corners.size();
    for (size_t i = 0; i < n; ++i) {
      const auto a = grid.GetCorner(cell.corners[i]);
      const auto b = grid.GetCorner(cell.corners[(i + 1) % n]);
      const auto d = grid.GetCorner(cell.corners[(i + 2) % n]);
      const auto turn = (b.x - a.x) * (d.y - b.y) - (b.y - a.y) * (d.x - b.x);
      // Shrinking a short side moves a corner by half its length, which can leave a turn a hair
      // the wrong way; anything more is a cell that is not convex.
      EXPECT_GT(turn, -1e-2f) << "cell " << c << " turns the wrong way at corner " << i;
    }
  }
}
TEST(IrregularGrid, LocatesEveryCellAtItsMiddle) {
  const auto grid = MakeGrid();
  for (CellIndex c = 0; c < grid.CellCount(); ++c)
    EXPECT_EQ(grid.Locate(grid.GetCell(c).centroid), c);
  EXPECT_EQ(grid.Locate({-50.f, -50.f}), INVALID_CELL);
}
TEST(IrregularGrid, IsTheSameForTheSameSeed) {
  const auto a = MakeGrid(3);
  const auto b = MakeGrid(3);
  const auto c = MakeGrid(4);
  ASSERT_EQ(a.CellCount(), b.CellCount());
  for (CellIndex i = 0; i < a.CellCount(); ++i)
    EXPECT_EQ(a.GetCell(i).centroid, b.GetCell(i).centroid);
  EXPECT_NE(a.GetCell(0).centroid, c.GetCell(0).centroid);
}
TEST(IrregularGrid, ListsEverySideOnce) {
  const auto grid = MakeGrid();
  size_t sides = 0;
  size_t shared = 0;
  for (const auto &cell : grid.GetCells()) {
    sides += cell.corners.size();
    shared += cell.neighbours.size();
  }
  // A shared side is two cells' and listed once; a rim side is one cell's.
  EXPECT_EQ(grid.GetEdges().size(), shared / 2 + (sides - shared));
  for (const auto &edge : grid.GetEdges()) {
    ASSERT_NE(edge.left, INVALID_CELL);
    if (edge.right != INVALID_CELL)
      EXPECT_TRUE(grid.AreNeighbours(edge.left, edge.right));
  }
  for (uint32_t corner = 0; corner < grid.GetCorners().size(); ++corner)
    EXPECT_FALSE(grid.GetCornerCells(corner).empty());
}
TEST(IrregularGrid, FootprintsAreDistinctJoinedCellsAtAnyAngle) {
  const auto grid = MakeGrid();
  // A building three cells long and one a cell wide at the far end: an L.
  const std::vector<glm::vec2> shape{{.0f, .0f}, {1.f, .0f}, {2.f, .0f}, {2.f, 1.f}};
  const glm::vec2 anchor{12.f, 9.f};
  for (auto step = 0; step < 16; ++step) {
    const auto angle = static_cast<float>(step) * .3927f;
    const auto cells = grid.Footprint(anchor, angle, shape);
    ASSERT_EQ(cells.size(), shape.size());
    EXPECT_EQ(cells.front(), grid.Locate(anchor));
    EXPECT_EQ(std::set<CellIndex>(cells.begin(), cells.end()).size(), cells.size());
    for (size_t i = 1; i < cells.size(); ++i) {
      const auto joined = std::any_of(cells.begin(), cells.begin() + static_cast<std::ptrdiff_t>(i), [&](const CellIndex other) { return grid.AreNeighbours(cells[i], other); });
      EXPECT_TRUE(joined) << "the footprint's cell " << i << " touches none before it";
    }
    // Turned, the far end lies the other way.
    const auto end = grid.GetCell(cells[2]).centroid - anchor;
    const auto wanted = glm::vec2(std::cos(angle), std::sin(angle));
    EXPECT_GT(glm::dot(glm::normalize(end), wanted), .5f);
  }
  EXPECT_TRUE(grid.Footprint({-10.f, -10.f}, .0f, shape).empty());
}
TEST(IrregularGrid, TakesPolygonsAsGiven) {
  // Two squares side by side and a triangle on top of the right one.
  const std::vector<glm::vec2> corners{{0, 0}, {1, 0}, {2, 0}, {0, 1}, {1, 1}, {2, 1}, {1.5f, 2}};
  const IrregularGrid grid(corners, {{0, 1, 4, 3}, {1, 2, 5, 4}, {4, 5, 6}});
  ASSERT_EQ(grid.CellCount(), 3u);
  EXPECT_TRUE(grid.AreNeighbours(0, 1));
  EXPECT_TRUE(grid.AreNeighbours(1, 2));
  EXPECT_FALSE(grid.AreNeighbours(0, 2));
  EXPECT_FLOAT_EQ(grid.GetCell(0).area, 1.f);
  EXPECT_EQ(grid.Locate({1.5f, 1.4f}), 2u);
  EXPECT_EQ(grid.Within(0, 1).size(), 2u);
  EXPECT_EQ(grid.Within(0, 2).size(), 3u);
}
