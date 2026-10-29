#pragma once
#include <cstdint>
#include <glm/ext/vector_float2.hpp>
#include <irregular_grid.hpp>
#include <kuki_engine_export.h>
#include <span>
#include <vector>
namespace kuki {
/// @brief A side of one polygon of a path's corridor that the path crosses into the next, as seen
/// walking across it: its left end and its right.
struct NavPortal {
  glm::vec2 left{};
  glm::vec2 right{};
};
/// @brief Where something can walk, as convex polygons that share sides, and the way across them.
///
/// A path is found in two steps. A* over the polygons finds the corridor -- the polygons to walk
/// through, each costing its own `cost` per unit crossed, a blocked one not to be entered -- and the
/// funnel then pulls a string tight through the sides the corridor crosses, which gives the
/// shortest line within it: straight across open ground, bending only at the corners of what is in
/// the way. Walking from polygon middle to polygon middle instead zigzags across every cell.
///
/// The polygons are a grid's cells when the mesh is made `FromGrid`, with the same indices, so a
/// map's movement layer is a cost and a flag per cell and nothing has to be traced or rasterised:
/// a building that takes a cell blocks that polygon, and a forest makes it slower. Anything else
/// convex will do as well -- a mesh carved from a level, a room's floor plan.
///
/// The search is in two dimensions. A map's heights are for the caller to put back under the path,
/// which is the same as asking the ground at each point.
class KUKI_ENGINE_API NavMesh {
public:
  NavMesh() = default;
  /// @brief A mesh of the given polygons, counter-clockwise, as indices into `vertices`.
  NavMesh(std::vector<glm::vec2> vertices, std::vector<std::vector<uint32_t>> polygons);
  /// @brief A mesh whose polygons are the grid's cells, polygon `i` being cell `i`.
  static auto FromGrid(const IrregularGrid &) -> NavMesh;
  auto PolygonCount() const -> size_t;
  auto GetPolygons() const -> const IrregularGrid &;
  /// @brief Whether a polygon may be walked through. A blocked polygon is never part of a path, not
  /// even at its start or end.
  auto SetBlocked(const uint32_t, const bool) -> void;
  auto IsBlocked(const uint32_t) const -> bool;
  /// @brief What crossing a polygon costs per unit of distance: one is open ground, two is ground
  /// that takes twice as long. Below a hundredth is taken as a hundredth.
  auto SetCost(const uint32_t, const float) -> void;
  auto GetCost(const uint32_t) const -> float;
  /// @brief The polygon a point is in, or `INVALID_CELL`.
  auto Locate(const glm::vec2 &) const -> uint32_t;
  /// @brief The polygons from one to another, both included, by the cheapest way between their
  /// middles. Empty when there is no way, or either end is blocked.
  auto FindCorridor(const uint32_t from, const uint32_t to) const -> std::vector<uint32_t>;
  /// @brief The sides a corridor crosses, each seen walking from one polygon into the next.
  auto Portals(const std::span<const uint32_t> corridor) const -> std::vector<NavPortal>;
  /// @brief The shortest line from one point to another through a corridor, as the points where it
  /// turns, both ends included.
  ///
  /// `clearance` keeps the line that far from the ends of every side it crosses, which is a body's
  /// radius kept clear of the corners it turns round. A side narrower than twice that is crossed at
  /// its middle.
  auto StringPull(const glm::vec2 &from, const glm::vec2 &to, const std::span<const uint32_t> corridor, const float clearance = .0f) const -> std::vector<glm::vec2>;
  /// @brief A path between two points: the corridor between the polygons they are in, pulled tight.
  /// Empty when either is off the mesh or no way joins them.
  auto FindPath(const glm::vec2 &from, const glm::vec2 &to, const float clearance = .0f) const -> std::vector<glm::vec2>;
private:
  IrregularGrid polygons;
  std::vector<float> costs;
  std::vector<bool> blocked;
  float cheapest{1.f};
};
} // namespace kuki
