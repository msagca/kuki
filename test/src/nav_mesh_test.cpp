#include <algorithm>
#include <glm/geometric.hpp>
#include <gtest/gtest.h>
#include <nav_mesh.hpp>
#include <vector>
using namespace kuki;
namespace {
constexpr int SIDE = 10;
/// @brief A ten by ten board of unit squares, square (x, y) being polygon y * 10 + x.
auto Board() -> NavMesh {
  std::vector<glm::vec2> corners;
  for (auto y = 0; y <= SIDE; ++y)
    for (auto x = 0; x <= SIDE; ++x)
      corners.emplace_back(static_cast<float>(x), static_cast<float>(y));
  const auto At = [](const int x, const int y) { return static_cast<uint32_t>(y * (SIDE + 1) + x); };
  std::vector<std::vector<uint32_t>> squares;
  for (auto y = 0; y < SIDE; ++y)
    for (auto x = 0; x < SIDE; ++x)
      squares.push_back({At(x, y), At(x + 1, y), At(x + 1, y + 1), At(x, y + 1)});
  return {corners, squares};
}
auto Square(const int x, const int y) -> uint32_t {
  return static_cast<uint32_t>(y * SIDE + x);
}
auto Length(const std::vector<glm::vec2> &path) -> float {
  auto total = .0f;
  for (size_t i = 1; i < path.size(); ++i)
    total += glm::distance(path[i - 1], path[i]);
  return total;
}
/// @brief Whether a path keeps out of every blocked polygon, looked at every twentieth of a unit.
auto KeepsOut(const NavMesh &mesh, const std::vector<glm::vec2> &path) -> bool {
  for (size_t i = 1; i < path.size(); ++i) {
    const auto steps = static_cast<int>(glm::distance(path[i - 1], path[i]) / .05f) + 1;
    for (auto k = 0; k <= steps; ++k) {
      const auto point = path[i - 1] + (path[i] - path[i - 1]) * (static_cast<float>(k) / static_cast<float>(steps));
      if (const auto polygon = mesh.Locate(point); polygon != INVALID_CELL && mesh.IsBlocked(polygon))
        return false;
    }
  }
  return true;
}
} // namespace
TEST(NavMesh, CrossesOpenGroundInAStraightLine) {
  const auto mesh = Board();
  const auto path = mesh.FindPath({.5f, .5f}, {9.5f, .7f});
  ASSERT_EQ(path.size(), 2u);
  EXPECT_EQ(path.front(), glm::vec2(.5f, .5f));
  EXPECT_EQ(path.back(), glm::vec2(9.5f, .7f));
}
TEST(NavMesh, PullsTheCorridorTight) {
  // Across the board corner to corner the cheapest corridor is a staircase of squares, and which
  // staircase is the search's to choose. Whichever it is, the line through it is far shorter than
  // the walk from square to square, and no longer than the way round its outside.
  const auto mesh = Board();
  const glm::vec2 from{.5f, .5f};
  const glm::vec2 to{9.5f, 6.5f};
  const auto corridor = mesh.FindCorridor(mesh.Locate(from), mesh.Locate(to));
  ASSERT_EQ(corridor.size(), 16u);
  const auto path = mesh.StringPull(from, to, corridor);
  EXPECT_EQ(path.front(), from);
  EXPECT_EQ(path.back(), to);
  EXPECT_LE(Length(path), 15.f);
  EXPECT_GE(Length(path), glm::distance(from, to) - 1e-4f);
}
TEST(NavMesh, GoesRoundAWallThroughItsGap) {
  auto mesh = Board();
  // A wall up the middle with one gap at the top.
  for (auto y = 0; y < SIDE - 1; ++y)
    mesh.SetBlocked(Square(5, y), true);
  const auto path = mesh.FindPath({1.5f, .5f}, {8.5f, .5f}, .25f);
  ASSERT_GE(path.size(), 4u);
  EXPECT_EQ(path.front(), glm::vec2(1.5f, .5f));
  EXPECT_EQ(path.back(), glm::vec2(8.5f, .5f));
  auto highest = .0f;
  for (const auto &point : path)
    highest = std::max(highest, point.y);
  // Through the gap, and kept a quarter of a unit off the wall's top as it turns over it.
  EXPECT_NEAR(highest, 9.25f, 1e-4f);
  EXPECT_TRUE(KeepsOut(mesh, path));
  // Up nine, across, down nine: the line can be no shorter, and is barely longer.
  EXPECT_LT(Length(path), 2.f * 9.f + 7.f);
}
TEST(NavMesh, PrefersTheCheaperWayRound) {
  auto mesh = Board();
  // A band of slow ground across the straight way.
  for (auto y = 0; y < 8; ++y)
    mesh.SetCost(Square(5, y), 20.f);
  const auto corridor = mesh.FindCorridor(Square(1, 2), Square(8, 2));
  ASSERT_FALSE(corridor.empty());
  for (const auto polygon : corridor)
    EXPECT_LT(mesh.GetCost(polygon), 2.f) << "walked through polygon " << polygon;
}
TEST(NavMesh, FindsNothingWhereThereIsNoWay) {
  auto mesh = Board();
  for (auto y = 0; y < SIDE; ++y)
    mesh.SetBlocked(Square(5, y), true);
  EXPECT_TRUE(mesh.FindPath({1.5f, .5f}, {8.5f, .5f}).empty());
  EXPECT_TRUE(mesh.FindPath({1.5f, .5f}, {5.5f, .5f}).empty());
  EXPECT_TRUE(mesh.FindPath({-1.f, .5f}, {1.5f, .5f}).empty());
}
TEST(NavMesh, WorksOnAnIrregularGrid) {
  GridSettings settings;
  settings.size = {20.f, 20.f};
  settings.seed = 21;
  const auto grid = IrregularGrid::Voronoi(settings);
  auto mesh = NavMesh::FromGrid(grid);
  // A wall of cells across the middle, but for the cells near the top.
  for (CellIndex c = 0; c < grid.CellCount(); ++c) {
    const auto &middle = grid.GetCell(c).centroid;
    if (std::abs(middle.x - 10.f) < 1.f && middle.y < 15.f)
      mesh.SetBlocked(c, true);
  }
  const auto path = mesh.FindPath({4.f, 5.f}, {16.f, 5.f}, .1f);
  ASSERT_GE(path.size(), 3u);
  // Round the end of the wall, and much shorter than walking the cells' middles would be.
  auto highest = .0f;
  for (const auto &point : path)
    highest = std::max(highest, point.y);
  EXPECT_GT(highest, 14.f);
  EXPECT_LT(Length(path), 12.f + 2.f * 11.f);
}
