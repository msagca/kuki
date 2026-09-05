#pragma once
#include <array>
#include <board.hpp>
#include <glm/ext/vector_float3.hpp>
#include <id.hpp>
#include <input_manager.hpp>
#include <script.hpp>
#include <string>
#include <vector>
namespace kuki {
class Application;
class EntityManager;
} // namespace kuki
/// @brief Everything the chess game does that is not a rule: geometry, picking, and turn flow.
///
/// A `Script`, which is the engine's one extension point for game code -- `ComponentType` is a
/// fixed enum in the engine, so a game cannot introduce a component of its own and has to keep its
/// state in a script instead. That is why this class holds the board rather than a component doing
/// so, and it is the right shape here anyway: there is exactly one of these in the scene.
class GameManager final : public kuki::Script {
public:
  GameManager();
  auto CloneTo(kuki::EntityManager &, const kuki::EntityID) const -> void override;
  auto GetTypeName() const -> std::string override;
  auto Start(kuki::Application &) -> void override;
  auto Update(kuki::Application &) -> void override;
  auto Shutdown(kuki::Application &) -> void override;
private:
  /// @brief Edge length of one square in world units, which is also the board's unit of everything.
  static constexpr float SQUARE_SIZE = 1.f;
  Board board;
  /// @brief The entity standing on each square, or an invalid id for an empty square.
  std::array<kuki::EntityID, Board::SquareCount> pieces{};
  /// @brief Flat markers showing where the selected piece may go, rebuilt on every selection.
  std::vector<kuki::EntityID> markers;
  int selected{-1};
  kuki::InputManager::ActionID clickAction{kuki::InputManager::InvalidActionID};
  kuki::InputManager::ActionID quitAction{kuki::InputManager::InvalidActionID};
  kuki::InputManager::ActionID restartAction{kuki::InputManager::InvalidActionID};
  /// @brief Centre of a square in world space, with `y` at the board's surface.
  static auto SquareCenter(const int) -> glm::vec3;
  /// @brief Which square an entity stands on, or -1 when it is neither a piece nor a square.
  auto SquareOfEntity(const kuki::EntityID) const -> int;
  auto BuildBoard() -> void;
  /// @brief Destroys every piece entity and creates them again from the board.
  ///
  /// Rebuilt wholesale rather than moved one at a time, because three of the rules move or remove
  /// something other than the piece that was clicked: castling moves a rook, en passant takes a
  /// pawn that is not on the destination, and promotion changes what a piece is drawn as. Handling
  /// each of those against a live entity map is three chances to leave the picture disagreeing
  /// with the position, and thirty-two entities is not a cost worth that risk.
  auto RebuildPieces() -> void;
  auto Select(const int) -> void;
  auto ClearMarkers() -> void;
  auto OnClick() -> void;
  auto Restart() -> void;
  /// @brief Puts the position's standing in the window's title bar.
  ///
  /// The title bar because the engine has no text rendering: there is no route to a glyph on the
  /// screen from here, and a status line that cannot be read is worse than one somewhere odd.
  auto UpdateTitle() const -> void;
};
