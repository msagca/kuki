#pragma once
#include <cstdint>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_int2.hpp>
#include <kuki_engine_export.h>
#include <limits>
#include <optional>
#include <span>
#include <utility>
#include <vector>
namespace kuki {
/// @brief A cell's place in an `IrregularGrid`.
using CellIndex = uint32_t;
inline constexpr CellIndex INVALID_CELL = std::numeric_limits<CellIndex>::max();
/// @brief One cell of an irregular grid: a convex polygon and the cells across each of its sides.
///
/// Corners go counter-clockwise with x to the right and y up. A grid lies on a plane, and which
/// plane is the caller's business; one laid on the ground with y as the world's z is seen from
/// above turned clockwise, which is a thing to remember when winding triangles from it.
struct GridCell {
  /// @brief The point the cell was grown from. Inside the cell, but not at its middle.
  glm::vec2 site{};
  /// @brief The middle of the polygon by area, which is where something placed "in the cell" goes.
  glm::vec2 centroid{};
  float area{};
  /// @brief Indices into the grid's corners, counter-clockwise.
  std::vector<uint32_t> corners;
  /// @brief The cell across each side, side `i` running from `corners[i]` to `corners[i + 1]`, or
  /// `INVALID_CELL` where the side is on the grid's rim.
  std::vector<CellIndex> across;
  /// @brief The valid entries of `across`, in the same order: every cell this one shares a side with.
  ///
  /// Two cells that only touch at a corner are not neighbours. On a grid of squares that is the
  /// difference between four neighbours and eight; here it is what makes the count vary from cell
  /// to cell at all.
  std::vector<CellIndex> neighbours;
};
/// @brief One side shared by two cells, or on the rim with a cell on one side only.
///
/// `left` is the cell that has `from` to `to` counter-clockwise; `right` the one across, which
/// sees the same side the other way round, or `INVALID_CELL`.
struct GridEdge {
  uint32_t from{};
  uint32_t to{};
  CellIndex left{INVALID_CELL};
  CellIndex right{INVALID_CELL};
};
/// @brief How `IrregularGrid::Voronoi` grows its cells.
struct GridSettings {
  /// @brief The rectangle from the origin that cells are grown in. A cell's site is always inside
  /// it; a cell at the rim may reach a little past it, and the rim is ragged rather than cut.
  glm::vec2 size{48.f, 48.f};
  /// @brief About how far apart neighbouring cells' middles are.
  float spacing{1.f};
  /// @brief Rounds of Lloyd's relaxation: each site moved to the middle of its cell and the cells
  /// grown again. Nought leaves the cells as uneven as the random points they came from; two or
  /// three makes them about the same size and nearly regular without making them a pattern. Every
  /// round makes the grid more like a honeycomb, and a honeycomb gives every cell six neighbours.
  int relaxation{2};
  /// @brief Sides shorter than this share of `spacing` are shrunk to a corner.
  ///
  /// Where four sites happen to lie nearly on one circle, the cells between them meet along a side
  /// too short to see -- two cells that are neighbours by the letter and touch at a corner by eye,
  /// and a sliver no building or path could make use of. Shrinking the side away settles it as the
  /// eye does. Nought keeps every side.
  float shortSide{.25f};
  /// @brief The fewest and most neighbours a cell away from the rim should have.
  ///
  /// The relaxation keeps nearly every cell between five and seven by itself. A side is not shrunk
  /// if that would leave either cell with fewer than the least, and a cell with more than the most
  /// gives up its shortest sides until it has no more -- unless that would take a neighbour below
  /// the least, in which case it keeps them, and the count is a little over. A cell at the rim has
  /// sides with nobody across and may have fewer.
  uint32_t minNeighbours{4};
  uint32_t maxNeighbours{8};
  uint64_t seed{};
};
/// @brief A plane cut into convex cells of different shapes and sizes, each with however many
/// neighbours its shape gives it.
///
/// What a square or hexagonal grid is to a board game, without the regularity: a cell is a polygon
/// of four to eight sides or so, its neighbours are the cells across them, and nothing about the
/// grid has a "north". That is the point of it -- a map cut this way does not read as tiles -- and
/// it is also what anything working on it must not assume: a neighbour has no direction but the
/// one its middle is in, and two cells' neighbour counts need not agree.
///
/// Grown as a Voronoi diagram of evenly scattered points (`Voronoi`), or taken from any set of
/// polygons that share corners (the constructor), which is how a grid made some other way -- a
/// relaxed quad mesh, a hand-drawn map -- becomes one of these.
///
/// Every query is by `CellIndex`. Point location is through a bucket grid over the cells' bounds,
/// so finding the cell under the cursor is a few polygon tests rather than one per cell.
class KUKI_ENGINE_API IrregularGrid {
public:
  IrregularGrid() = default;
  /// @brief A grid of the given polygons, each a list of indices into `corners`, counter-clockwise.
  ///
  /// Two polygons are neighbours where one has a side from `p` to `q` and the other from `q` to `p`.
  /// The polygons are expected to be convex: point location assumes it, and so does a navigation
  /// mesh built on the grid.
  IrregularGrid(std::vector<glm::vec2> corners, std::vector<std::vector<uint32_t>> polygons);
  /// @brief Grows a grid of Voronoi cells over a rectangle. The same settings give the same grid.
  static auto Voronoi(const GridSettings &) -> IrregularGrid;
  auto CellCount() const -> size_t;
  auto GetCell(const CellIndex) const -> const GridCell &;
  auto GetCells() const -> std::span<const GridCell>;
  auto GetCorner(const uint32_t) const -> glm::vec2;
  auto GetCorners() const -> std::span<const glm::vec2>;
  /// @brief The cells that have a corner among theirs, in no particular order.
  auto GetCornerCells(const uint32_t) const -> std::span<const CellIndex>;
  /// @brief Every side once, rim sides included.
  auto GetEdges() const -> std::span<const GridEdge>;
  /// @brief The square root of a cell's average area: the length the grid is cut at.
  auto GetSpacing() const -> float;
  auto GetMin() const -> glm::vec2;
  auto GetMax() const -> glm::vec2;
  auto AreNeighbours(const CellIndex, const CellIndex) const -> bool;
  /// @brief Whether a cell has a side on the rim, with nothing across it.
  auto IsRim(const CellIndex) const -> bool;
  /// @brief The side two neighbouring cells share, as its two ends, or nothing if they share none.
  auto SharedSide(const CellIndex, const CellIndex) const -> std::optional<std::pair<glm::vec2, glm::vec2>>;
  auto Contains(const CellIndex, const glm::vec2 &) const -> bool;
  /// @brief The cell a point is in, or `INVALID_CELL` if it is in none.
  auto Locate(const glm::vec2 &) const -> CellIndex;
  /// @brief The cell whose middle is nearest a point, inside the grid or out of it.
  auto Nearest(const glm::vec2 &) const -> CellIndex;
  /// @brief Every cell within `steps` neighbour-to-neighbour steps of one, the cell itself first and
  /// the rest in order of steps.
  auto Within(const CellIndex, const int steps) const -> std::vector<CellIndex>;
  /// @brief The cells a shape covers when it is put down at a point and turned to an angle.
  ///
  /// The shape is a list of points, in units of `GetSpacing` and the shape's own axes, which turn by
  /// `angle` radians counter-clockwise. The first point is the one put down at `anchor`: its cell is
  /// the first of the answer. Each point after it takes the cell, among those beside the ones already
  /// taken, whose middle is nearest where the point falls -- so the cells are always distinct and
  /// joined, however the shape is turned and however oddly the cells under it are cut, and a shape
  /// of `n` points always covers `n` cells. Nothing is returned if there are not enough cells: the
  /// first point is off the grid, or the rest run off it.
  ///
  /// What a building of several cells needs. A building free to turn to any angle cannot ask for
  /// "this cell and the one to its east", because an irregular grid has no east; it can ask for the
  /// cells nearest where its corners would be, which is what this answers.
  auto Footprint(const glm::vec2 &anchor, const float angle, const std::span<const glm::vec2> shape) const -> std::vector<CellIndex>;
  /// @brief The middle of several cells by area: where something that covers them stands.
  auto Middle(const std::span<const CellIndex>) const -> glm::vec2;
  /// @brief The fewest and the most neighbours any cell has, cells on the rim left out.
  auto NeighbourRange() const -> std::pair<uint32_t, uint32_t>;
private:
  std::vector<glm::vec2> corners;
  std::vector<GridCell> cells;
  std::vector<GridEdge> edges;
  /// The cells around each corner, flattened: corner `i`'s are `cornerCells[cornerOffsets[i]]` on.
  std::vector<uint32_t> cornerOffsets;
  std::vector<CellIndex> cornerCells;
  glm::vec2 min{};
  glm::vec2 max{};
  float spacing{1.f};
  /// The bucket grid point location searches: each bucket lists the cells whose bounds overlap it.
  glm::ivec2 bucketCount{};
  float bucketSize{1.f};
  std::vector<uint32_t> bucketOffsets;
  std::vector<CellIndex> bucketCells;
  auto Build() -> void;
  auto BucketOf(const glm::vec2 &) const -> glm::ivec2;
};
} // namespace kuki
