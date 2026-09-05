#pragma once
#include <array>
#include <board.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <id.hpp>
#include <input_manager.hpp>
#include <script.hpp>
#include <string>
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
  ///
  /// The squares are drawn at exactly this and butt up against one another, so the board reads as
  /// one surface. What tells a square from its neighbour is the colour and, where the selected
  /// piece may go, the height.
  static constexpr float SQUARE_SIZE = 1.f;
  /// @brief Thickness of the slab a square is drawn as. Its upper face is the board's surface.
  static constexpr float SQUARE_HEIGHT = .2f;
  /// @brief How far a square one step from the selected piece stands up, and how far the furthest.
  ///
  /// The whole of the legal-move display. Nothing is laid over the board and no square changes
  /// colour: a destination is a square that is raised, which reads from any angle the camera can
  /// reach and puts nothing in front of the square for a click to hit instead.
  ///
  /// Scaled by how far the move travels, so the shape of a piece's reach is legible before any of
  /// the squares are read individually -- a rook down an open file climbs away from you, a pawn
  /// barely stirs. Slight on purpose at both ends: the point is to be read at a glance, and a board
  /// that heaves by a fifth of a square to answer a click looks broken rather than helpful.
  static constexpr float SQUARE_RISE_MIN = .04f;
  static constexpr float SQUARE_RISE_MAX = .13f;
  /// @brief Distance, in squares, at which the rise reaches its maximum.
  ///
  /// Seven is the length of the longest move along a rank or a file. A bishop crossing corner to
  /// corner covers half again as much ground and simply tops out, which is the right answer: past
  /// the width of the board the exact figure has stopped telling anyone anything.
  static constexpr float SQUARE_RISE_RANGE = 7.f;
  /// @brief How far the selected piece lifts off the square it stands on.
  static constexpr float SELECT_LIFT = .25f;
  Board board;
  /// @brief The slab drawn for each square. Created once, and after that only ever moved.
  std::array<kuki::EntityID, Board::SquareCount> squares{};
  /// @brief The entity standing on each square, or an invalid id for an empty square.
  std::array<kuki::EntityID, Board::SquareCount> pieces{};
  /// @brief How far each square stands raised. Zero everywhere the selected piece may not go.
  ///
  /// The height rather than a flag, because the height is what varies: it is worked out once when
  /// the selection changes and read back by everything that has to stand on the square.
  std::array<float, Board::SquareCount> rise{};
  /// @brief For each square holding a piece the camera cannot see past, the square it is hiding.
  ///
  /// -1 everywhere else. An opponent's piece that is not itself a target has nothing to say to a
  /// click, and standing in front of a square that does is the one way it can get in the way -- so
  /// it is drawn see-through and hands the click on to whatever is behind it.
  ///
  /// Which pieces those are depends on where the camera is, not only on what is selected, so this
  /// is worked out every frame. See `UpdateOcclusion`.
  std::array<int, Board::SquareCount> hidden{};
  /// @brief Which squares hold a piece the selected piece may take, and so which ones are red.
  ///
  /// Kept apart from `rise` because the two are not the same set. En passant takes a pawn that is
  /// not on the destination, so the square that rises and the square that turns red are different
  /// ones -- and `Move::captured` is a square rather than a flag for exactly that reason.
  std::array<bool, Board::SquareCount> capturable{};
  int selected{-1};
  /// @brief Whether the position still has a move in it.
  ///
  /// Cached rather than asked for, because `Board::State` generates every move for both sides to
  /// answer and the occlusion pass would otherwise ask it once a frame. Refreshed by `UpdateTitle`,
  /// which is called at exactly the points the board changes and needs the answer anyway.
  bool playable{true};
  /// @brief Whether the button is being held on a press the board had no answer for.
  ///
  /// A press that selects or moves belongs to the board; anything else belongs to the camera, and
  /// holding it turns the view. Decided on the press rather than once the mouse has travelled, so
  /// that a move is never also a nudge of the camera when the mouse slips a pixel under the click.
  bool orbiting{};
  glm::vec2 orbitLast{};
  /// @brief Where the camera sits on its sphere about the board, and how far out.
  ///
  /// Read off the camera when a drag begins rather than kept in step with it, so that anything else
  /// moving the camera is picked up rather than fought.
  float orbitYaw{};
  float orbitPitch{};
  float orbitDistance{};
  kuki::InputManager::ActionID pressAction{kuki::InputManager::InvalidActionID};
  kuki::InputManager::ActionID releaseAction{kuki::InputManager::InvalidActionID};
  kuki::InputManager::ActionID quitAction{kuki::InputManager::InvalidActionID};
  kuki::InputManager::ActionID restartAction{kuki::InputManager::InvalidActionID};
  /// @brief Centre of a square in world space, with `y` at the board's unraised surface.
  static auto SquareCenter(const int) -> glm::vec3;
  /// @brief Height of a square's upper face, which is what anything standing on it stands on.
  auto SquareTop(const int) const -> float;
  /// @brief Which square an entity stands on, or -1 when it is neither a piece nor a square.
  auto SquareOfEntity(const kuki::EntityID) const -> int;
  /// @brief Draws the board and the pieces the way the position and the selection say they are.
  auto Refresh() -> void;
  /// @brief Places every square, creating the slabs the first time through.
  auto RefreshSquares() -> void;
  /// @brief Brings the drawn pieces back in line with the board.
  ///
  /// Read off the board rather than moved one at a time, because three of the rules move or remove
  /// something other than the piece that was clicked: castling moves a rook, en passant takes a
  /// pawn that is not on the destination, and promotion changes what a piece is drawn as. Handling
  /// each of those against a live entity map is three chances to leave the picture disagreeing
  /// with the position.
  ///
  /// What it does not do is destroy the entities and make them again. Picking reads the id buffer
  /// the frame before it left behind, so an entity replaced between that frame and the click
  /// answers with an id that no longer resolves -- and since selecting a piece calls this to lift
  /// that piece, every one of the thirty-two would be replaced in order to move one. A second click
  /// arriving within a frame of the first then found nothing and deselected. Each square is
  /// compared against what stands on it instead, so a piece that has not changed keeps its id.
  auto RefreshPieces() -> void;
  auto Select(const int) -> void;
  /// @brief Works out which of the opponent's pieces stand between the camera and something the
  /// next click wants to reach.
  ///
  /// What that is depends on whether a piece is selected. With one, it is the squares that piece
  /// may move to; with none, it is this side's own pieces -- because being unable to pick up a
  /// knight because an enemy pawn is in front of it is the same problem, with the same answer.
  ///
  /// Each piece is bounded by the upright cylinder it is drawn inside, and a line is run from the
  /// camera to the point on each target a click would be aimed at. A generous bound is the right
  /// way to be wrong: a piece that only nearly covers a target still hides enough of it to be worth
  /// seeing past.
  ///
  /// @return Whether the set changed, and so whether the pieces need drawing again.
  auto UpdateOcclusion() -> bool;
  /// @brief Answers a press: a move, a selection, or the beginning of a drag of the camera.
  auto OnPress() -> void;
  /// @brief Takes the camera's current place on its sphere and starts following the mouse.
  auto BeginOrbit() -> void;
  /// @brief Moves the camera along its sphere by however far the mouse has travelled this frame.
  auto Orbit(kuki::Application &) -> void;
  auto Restart() -> void;
  /// @brief Puts the position's standing in the window's title bar.
  ///
  /// The title bar because the engine has no text rendering: there is no route to a glyph on the
  /// screen from here, and a status line that cannot be read is worse than one somewhere odd.
  ///
  /// Also where `playable` is refreshed, since working out what to write is working out whether
  /// the game is over.
  auto UpdateTitle() -> void;
};
