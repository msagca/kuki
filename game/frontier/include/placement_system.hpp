#pragma once
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <id.hpp>
#include <irregular_grid.hpp>
#include <optional>
#include <system.hpp>
#include <vector>
namespace kuki {
class Application;
} // namespace kuki
/// @brief Putting buildings on the map: choosing one, turning it, and setting it down on free cells.
///
/// A number key picks a building and shows it under the cursor, see-through: green where it can go
/// and red where it cannot, with its cells outlined on the ground. Q and E turn it -- to any angle,
/// since a cell has no sides to line up with -- and which cells it covers follows the turn: the
/// grid's `Footprint` finds the cells nearest where each of its own lies. A left click sets it down
/// if every cell is free; a right click or Escape puts it away. Delete, or X, over a building
/// takes it down.
///
/// Setting one down takes its cells in the building layer, blocks them in the movement layer and
/// in the navigation mesh, and a windmill's sails turn from then on as hard as the wind blows.
class PlacementSystem final : public kuki::System {
public:
  explicit PlacementSystem(kuki::Application &);
  auto Update(const float) -> void override;
  /// @brief Every building standing, for the ambience to smoke and turn.
  struct Placed {
    int type{};
    std::vector<kuki::CellIndex> cells;
    glm::vec3 position{};
    float angle{};
    std::vector<kuki::EntityID> parts;
    /// The sails, if it has any, and how far round they have turned.
    kuki::EntityID sails{};
    float spin{};
    kuki::EntityID smoke{};
  };
  auto GetPlaced() const -> const std::vector<std::optional<Placed>> &;
private:
  std::vector<std::optional<Placed>> placed;
  /// The building being placed, or minus one.
  int selected{-1};
  float angle{};
  std::vector<kuki::EntityID> ghost;
  std::vector<kuki::CellIndex> footprint;
  bool valid{};
  uint32_t generation{};
  auto Select(const int) -> void;
  auto ClearGhost() -> void;
  auto Build(const int type, const std::vector<kuki::CellIndex> &, const float) -> void;
  auto Demolish(const int) -> void;
  /// @brief Where a building standing on cells goes: the middle of the cells, on the highest of them.
  auto Stand(const std::vector<kuki::CellIndex> &) const -> glm::vec3;
  auto Spin(const float) -> void;
  auto DrawPanel() -> void;
  /// @brief Puts down two of every building, wherever they fit, for `--demo`.
  auto Demo() -> void;
};
