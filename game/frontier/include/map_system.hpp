#pragma once
#include <array>
#include <command_line.hpp>
#include <cstdint>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <id.hpp>
#include <optional>
#include <random.hpp>
#include <span>
#include <string>
#include <system.hpp>
#include <terrain.hpp>
#include <vector>
namespace kuki {
class Application;
} // namespace kuki
/// @brief Which of the map's logical layers is laid over the ground, if any.
enum class Layer : uint8_t {
  None,
  Cells,
  Resources,
  Movement,
  Building
};
inline constexpr std::array LAYER_NAMES{"None", "Cells", "Resources", "Movement", "Building"};
/// @brief The map: generates it, builds it into the scene, and shows its layers.
///
/// On starting, a map is made from the seed `--seed` gives, or one from the clock, and built:
/// a mesh a terrain for the ground, the faces of the steps between cells, the sea in three depths
/// and its foam along the shore, and the props -- trees, grass, stones, crystals -- each an
/// entity. G makes another map from a new seed in its place. Tab steps through the layers drawn over
/// the ground: the cells' outlines, the resources, what it costs to walk where, and where a building
/// can go. A right click marks one end of a path and a second the other, and the path the navigation
/// mesh finds between them is drawn on the ground -- the movement layer at work, with nobody yet to
/// walk it.
///
/// The cell under the cursor is found here, for every system that needs it, and what it is shown in
/// a panel in the corner.
class MapSystem final : public kuki::System {
public:
  static constexpr kuki::Flag SEED_FLAG{"--seed", kuki::FlagKind::Value, "<number>", "the seed the first map is made from; the same seed makes the same map"};
  static constexpr kuki::Flag LAYER_FLAG{"--layer", kuki::FlagKind::Value, "<name>", "the layer drawn over the ground to begin with: cells, resources, movement or building"};
  /// @brief Lays a path across the map on starting, as a right click at each end would.
  static constexpr kuki::Flag DEMO_FLAG{"--demo", kuki::FlagKind::Switch, "", "start with a path across the map and a few buildings put down"};
  static constexpr std::array FLAGS{SEED_FLAG, LAYER_FLAG, DEMO_FLAG};
  explicit MapSystem(kuki::Application &);
  auto PostStart() -> void override;
  auto Update(const float) -> void override;
  auto GetMap() -> frontier::TerrainMap &;
  /// @brief Where the cursor meets the ground or the sea, and the cell there.
  auto GetCursor() const -> const std::optional<glm::vec3> &;
  auto GetHoveredCell() const -> kuki::CellIndex;
  /// @brief Draws an outline round cells, in a colour: a building's footprint, the cell under the
  /// cursor. Lasts until it is asked for again; an empty list clears it.
  auto Highlight(const std::span<const kuki::CellIndex>, const glm::vec3 &colour) -> void;
  /// @brief Says the building or movement layers have changed, so the drawn layer is made again.
  auto RefreshLayers() -> void;
  /// @brief Whether the cursor is held by something else -- a building being placed -- so right
  /// clicks are not taken for the ends of a path.
  auto SetPathEnabled(const bool) -> void;
  /// @brief Counts the maps made, so a system that places things on one can tell a new one has come.
  auto GetGeneration() const -> uint32_t;
  /// @brief Where the camera opens on a map: open land near the middle, room to build round it.
  auto GetStart() const -> glm::vec2;
  /// @brief The point where a line from `origin` along `direction` first meets the ground or the sea.
  auto Raycast(const glm::vec3 &origin, const glm::vec3 &direction) const -> std::optional<glm::vec3>;
private:
  frontier::TerrainMap map;
  uint64_t seed{1};
  uint32_t generation{};
  Layer layer{Layer::None};
  bool layersDirty{true};
  std::vector<kuki::EntityID> entities;
  glm::vec2 start{};
  std::optional<glm::vec3> cursor;
  kuki::CellIndex hovered{kuki::INVALID_CELL};
  std::vector<kuki::CellIndex> highlighted;
  glm::vec3 highlightColour{1.f};
  bool pathEnabled{true};
  std::optional<glm::vec2> pathStart;
  std::vector<glm::vec2> path;
  float pathLength{};
  /// @brief Makes a map from `seed` and puts it in the scene in place of the last.
  auto Generate() -> void;
  auto Clear() -> void;
  auto BuildGround() -> void;
  auto BuildWater() -> void;
  auto BuildProps() -> void;
  auto BuildLayer() -> void;
  auto BuildPath() -> void;
  /// @brief Places a mesh with a material, standing at a point, turned about the vertical and scaled.
  auto Place(const std::string &mesh, const std::string &material, const glm::vec3 &, const float yaw = .0f, const float scale = 1.f) -> kuki::EntityID;
  /// @brief A point inside a cell, picked at random but not too near its edge.
  auto Scatter(const kuki::CellIndex, kuki::Random &, const float reach = .6f) const -> glm::vec2;
  auto DrawPanel() -> void;
  /// @brief A path between the two walkable cells furthest apart of a few picked at random.
  auto DemoPath() -> void;
};
