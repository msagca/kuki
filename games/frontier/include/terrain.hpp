#pragma once
#include <array>
#include <cstdint>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <irregular_grid.hpp>
#include <nav_mesh.hpp>
#include <string_view>
#include <vector>
namespace frontier {
/// @brief What the ground of a cell is: the tiles the wave function collapse chooses between.
///
/// In order of height, which is also the order the rules join them in: every tile may meet the
/// ones beside it in this list and few others, so the sea becomes land by way of a shore and a
/// meadow becomes a mountain by way of the hills.
enum class Terrain : uint8_t {
  Deep,
  Shallows,
  Sand,
  Grass,
  Meadow,
  Forest,
  Hills,
  Rock,
  Snow
};
inline constexpr size_t TERRAIN_COUNT = 9;
/// @brief What a cell holds that can be gathered. A site covers a whole cell, or several.
enum class Resource : uint8_t {
  None,
  Timber,
  Stone,
  Ore,
  Fish,
  Berries
};
inline constexpr size_t RESOURCE_COUNT = 6;
struct TerrainInfo {
  std::string_view name;
  /// @brief The height of the cell's top, sea level being nought.
  float height;
  /// @brief What crossing the cell costs on foot, open grass being one; nought where nobody walks.
  float cost;
  /// @brief Whether a building may stand on it.
  bool buildable;
  /// @brief The height the elevation field likes this tile at, from nought to one, for the bias the
  /// collapse is given. See `MapSettings`.
  float elevation;
};
inline constexpr std::array<TerrainInfo, TERRAIN_COUNT> TERRAIN{{
  {"Deep water", -1.3f, .0f, false, .1f},
  {"Shallows", -.42f, .0f, false, .32f},
  {"Sand", .14f, 1.3f, true, .38f},
  {"Grass", .32f, 1.f, true, .5f},
  {"Meadow", .36f, 1.f, true, .58f},
  {"Forest", .4f, 1.9f, false, .67f},
  {"Hills", .9f, 1.6f, true, .85f},
  {"Rock", 1.3f, .0f, false, .95f},
  {"Snow", 1.6f, .0f, false, .99f},
}};
inline constexpr std::array<std::string_view, RESOURCE_COUNT> RESOURCE_NAMES{"None", "Timber", "Stone", "Ore", "Fish", "Berries"};
auto Info(const Terrain) -> const TerrainInfo &;
auto IsWater(const Terrain) -> bool;
/// @brief Whether the top of a cell of this terrain is a peak rather than a flat terrace.
auto IsPeaked(const Terrain) -> bool;
/// @brief A deposit of something to gather, over one cell or several that are neighbours.
struct ResourceSite {
  Resource kind{};
  std::vector<kuki::CellIndex> cells;
  /// @brief Where the deposit's middle is and which way it lies, for whatever draws it.
  glm::vec2 middle{};
  float angle{};
  int amount{};
};
/// @brief How a map is made.
struct MapSettings {
  uint64_t seed{1};
  /// @brief The rectangle the cells are grown over, and how far apart they are.
  glm::vec2 size{72.f, 52.f};
  float spacing{1.35f};
  /// @brief How strongly the elevation field leans the collapse towards a tile, and how wide the band
  /// of elevation is that each tile likes.
  float bias{30.f};
  float bandWidth{.075f};
};
/// @brief A generated map: its cells, the ground of each, and the layers the game reads off it.
///
/// Every layer is per cell and indexed by `kuki::CellIndex`. The ground is what the collapse chose;
/// the resources are sites laid over it; movement is a cost per cell and the navigation mesh made
/// from it; building is whether a cell is free to build on and what stands there if not. A building
/// or a deposit takes whole cells, so every layer is exactly as fine as the grid.
struct TerrainMap {
  uint64_t seed{};
  kuki::IrregularGrid grid;
  std::vector<Terrain> terrain;
  /// @brief The height of each cell's top: its terrace, or the foot of its peak.
  std::vector<float> height;
  /// @brief How far the middle of a peaked cell stands above its rim. Nought for a flat cell.
  std::vector<float> peak;
  /// @brief The height of each of the grid's corners: a flat cell's corners are at its own height,
  /// and a peak's are shared with whatever is round it, so mountains meet as one surface.
  std::vector<float> cornerHeight;
  /// @brief The resource site on each cell, as an index into `sites`, or minus one.
  std::vector<int> resource;
  std::vector<ResourceSite> sites;
  /// @brief The building on each cell, or minus one.
  std::vector<int> occupant;
  kuki::NavMesh nav;
  /// @brief How the collapse went: the repairs it made and the attempts it took.
  uint32_t repairs{};
  uint32_t attempts{};
  float elapsed{};
  auto CellCount() const -> size_t;
  /// @brief The height of the ground at a point: a terrace's top or a peak's slope, and minus
  /// infinity off the map.
  auto GroundAt(const glm::vec2 &) const -> float;
  /// @brief A cell's edge heights, from its side `i`'s start to its end: both its height for a
  /// terrace, the corners' for a peak.
  auto SideHeights(const kuki::CellIndex, const size_t side) const -> std::pair<float, float>;
  /// @brief Whether a building may be put on a cell: buildable ground, nothing to gather, nothing
  /// built, and not at the edge of the map.
  auto IsFree(const kuki::CellIndex) const -> bool;
  /// @brief What walking across a cell costs now, buildings and deposits included; nought where it
  /// cannot be crossed.
  auto MoveCost(const kuki::CellIndex) const -> float;
  /// @brief Brings the navigation mesh up to date with one cell's movement cost.
  auto RefreshNavigation(const kuki::CellIndex) -> void;
  /// @brief Whether a land cell touches the water.
  auto IsCoast(const kuki::CellIndex) const -> bool;
};
/// @brief Makes a map: grows the cells, collapses the terrain over them, raises the ground, lays
/// out the resources and builds the movement layer. The same settings make the same map.
auto GenerateMap(const MapSettings &) -> TerrainMap;
/// @brief A point on the map's plane as a point in the world, at a height.
inline auto ToWorld(const glm::vec2 &point, const float height) -> glm::vec3 {
  return {point.x, height, point.y};
}
} // namespace frontier
