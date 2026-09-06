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
  /// @brief How far the selected piece lifts off the square it stands on.
  static constexpr float SELECT_LIFT = .25f;
  /// @brief Height of one line of label text in world units, which is the em the glyphs are laid
  /// out in.
  ///
  /// A third of a square. Large enough to read from the camera's resting position, small enough
  /// that the labels stay a caption on the board rather than a second thing on the table.
  static constexpr float LABEL_SIZE = .42f;
  /// @brief Clear space between the board's edge and the band the labels are centred in.
  ///
  /// The labels are centred in a band one `LABEL_SIZE` deep beginning this far out, and no glyph
  /// fills its em, so the gap that is actually seen is this plus whatever the ink leaves.
  static constexpr float LABEL_MARGIN = .16f;
  /// @brief Height of the options text, in pixels, and of the clocks, which are larger.
  ///
  /// There is a hierarchy between the two and this is it: a clock is read over and over while a
  /// game is played, and a time control is read once when it is chosen. The gap is small on
  /// purpose -- enough that the eye goes to the clock first, not so much that the options read as
  /// a footnote to it.
  ///
  /// Pixels rather than anything derived from the window, so the text stays the size it was drawn
  /// at whatever the window is doing. A caption that grew with the window would be a caption that
  /// is unreadable on a small one and absurd on a large one.
  ///
  /// Both are comfortably under twice the pixel height the atlas is baked at, which is where a
  /// bitmap font starts to soften -- and the 2x oversampling in `Font::Load` means the atlas holds
  /// about twice the detail its nominal size suggests.
  static constexpr float OPTION_TEXT_SIZE = 52.f;
  static constexpr float CLOCK_TEXT_SIZE = OPTION_TEXT_SIZE * 1.2f;
  /// @brief The time controls on offer, in minutes, and the increments, in seconds.
  ///
  /// Arrays rather than a range, because these are the figures people actually play: bullet,
  /// blitz, rapid. A slider over every whole number between them would offer a thousand controls
  /// nobody wants in order to reach the six they do.
  static constexpr int TIME_OPTIONS[]{1, 3, 5, 10, 15, 30};
  static constexpr int INCREMENT_OPTIONS[]{0, 1, 2, 5, 10};
  /// @brief Keys the chosen control is kept under between runs. See `kuki::Preferences`.
  static constexpr const char *PREF_MINUTES = "clock.minutes";
  static constexpr const char *PREF_INCREMENT = "clock.increment";
  /// @brief Inset of the options from the window's top left corner, and the gap between options.
  static constexpr float OPTION_MARGIN = 40.f;
  static constexpr float OPTION_SPACING = 22.f;
  /// @brief Distance from one row of options to the next.
  static constexpr float OPTION_ROW_STEP = OPTION_TEXT_SIZE * 1.15f;
  /// @brief How many recent moves are kept for the title bar.
  ///
  /// More than will usually fit, because what fits is decided in characters by
  /// `TITLE_MOVE_BUDGET` rather than in moves: `O-O-O` and `e4` are not the same width.
  static constexpr size_t MOVE_HISTORY = 12;
  /// @brief Characters of the title given over to moves.
  ///
  /// A title bar is one line and cannot scroll, so without a limit the moves walk the name of the
  /// game off the left of it. Sixty reads comfortably beside a title in a window of ordinary width;
  /// past that the bar elides the middle and the whole row stops being worth reading. Older moves
  /// fall off the end rather than the name giving way.
  static constexpr size_t TITLE_MOVE_BUDGET = 60;
  /// @brief Base of the ids the option rows answer `PickOverlay` with.
  ///
  /// A row's ids run from its base, so an id says which row was clicked and which option in it.
  /// Both are well clear of `Overlay::NoHit`, which is zero.
  static constexpr int TIME_OPTION_ID = 100;
  static constexpr int INCREMENT_OPTION_ID = 200;
  /// @brief Inset of the clocks from the right edge, in pixels.
  static constexpr float CLOCK_MARGIN = 40.f;
  /// @brief How far each clock's middle sits from the window's, so the two face one another.
  ///
  /// A shade under one line each side of the centre, which leaves about as much clear space
  /// between the two as a digit is tall -- enough to read them as two clocks rather than one
  /// stacked number, and close enough to read as a pair.
  static constexpr float CLOCK_GAP = CLOCK_TEXT_SIZE * .7f;
  /// @brief Seconds remaining below which a clock turns red.
  static constexpr float CLOCK_LOW = 30.f;
  Board board;
  /// @brief The slab drawn for each square. Created once, and after that only ever moved.
  std::array<kuki::EntityID, Board::SquareCount> squares{};
  /// @brief Every rank and file label, as one entity. Created once and never touched again.
  kuki::EntityID labels{};
  /// @brief The entity standing on each square, or an invalid id for an empty square.
  std::array<kuki::EntityID, Board::SquareCount> pieces{};
  /// @brief Which squares the selected piece may move to, and so which ones are green.
  ///
  /// The destinations rather than everything the move touches: en passant takes a pawn that is not
  /// standing on the destination, so the square that turns green and the square that turns red are
  /// different ones there. See `capturable`.
  std::array<bool, Board::SquareCount> reachable{};
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
  /// Kept apart from `reachable` because the two are not the same set, and en passant is why: the
  /// pawn it takes is not standing on the destination, so the square that is clicked to make the
  /// move is green and the square holding the piece that will disappear is red. `Move::captured`
  /// is a square rather than a flag for exactly that reason.
  ///
  /// For every other capture the two coincide, and red wins -- a square that can be moved to by
  /// taking what stands on it has more to say about the piece than about the square.
  std::array<bool, Board::SquareCount> capturable{};
  int selected{-1};
  /// @brief The position's standing, as `UpdateTitle` last worked it out.
  ///
  /// Cached because `Board::State` generates every move for both sides to answer, and the king's
  /// colour is read every time the pieces are drawn -- which is whenever the occlusion changes,
  /// and so potentially every frame. See `UpdateTitle` for when it is refreshed.
  GameState state{GameState::Playing};
  /// @brief Seconds left on each side's clock, White first.
  ///
  /// Seeded from the first control on offer so that a clock is never momentarily zero, which is
  /// the reading that means a flag has fallen. `Start` puts the chosen control on it before the
  /// first frame either way.
  std::array<float, 2> clocks{TIME_OPTIONS[0] * 60.f, TIME_OPTIONS[0] * 60.f};
  /// @brief Which of `TIME_OPTIONS` and `INCREMENT_OPTIONS` is in force.
  size_t timeOption{};
  size_t incrementOption{};
  /// @brief The last few moves in algebraic notation, oldest first, empty where none was played.
  std::array<std::string, MOVE_HISTORY> moves{};
  /// @brief Index of the side whose flag has fallen, or -1 while both are still running.
  ///
  /// Kept apart from `playable` because the position cannot answer for it: `Board::State` looks at
  /// the pieces, and a game lost on time is one the pieces say nothing about.
  int flagged{-1};
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
  /// @brief Centre of a square in world space, with `y` on the board's surface.
  static auto SquareCenter(const int) -> glm::vec3;
  /// @brief Which square an entity stands on, or -1 when it is neither a piece nor a square.
  auto SquareOfEntity(const kuki::EntityID) const -> int;
  /// @brief Draws the board and the pieces the way the position and the selection say they are.
  auto Refresh() -> void;
  /// @brief Places every square, creating the slabs the first time through.
  auto RefreshSquares() -> void;
  /// @brief Bakes a font and lays the files and ranks around two edges of the board.
  ///
  /// Everything the labels need is built here rather than staged as assets, because none of it can
  /// be: a `.mat` file cannot name a texture, and the texture in question does not exist until the
  /// font has been baked. The atlas, the material that samples it and the mesh of glyph quads are
  /// therefore made in code and handed to the asset manager under names, which is what
  /// `Application::AddAsset` is for.
  ///
  /// One mesh and one entity for all sixteen labels. They never move -- a label is a property of
  /// the board rather than of the position -- so there is nothing to be gained by keeping them
  /// apart, and a good deal of draw call to be saved by not doing so.
  ///
  /// Files along the near edge and ranks along the left, as a printed diagram has them. Not on all
  /// four edges: the board turns under the camera, so half of any second pair would be upside down
  /// wherever the first pair was not, and two labelled edges is what a board actually carries.
  auto CreateLabels() -> void;
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
  /// @brief Reads the saved time control, or leaves the defaults where none was saved.
  auto LoadSettings(kuki::Application &) -> void;
  /// @brief Writes the chosen time control back, to be picked up next launch.
  auto SaveSettings(kuki::Application &) -> void;
  /// @brief Seconds on a fresh clock under the chosen control.
  auto StartSeconds() const -> float;
  /// @brief Seconds added to a side's clock when it completes a move.
  auto IncrementSeconds() const -> float;
  /// @brief Adds a move to the list and drops the oldest.
  auto PushMove(std::string) -> void;
  /// @brief Queues both rows of options down the left, and says where each one was put.
  auto DrawOptions(kuki::Application &) -> void;
  /// @brief Answers a click on an option, restarting under the new control.
  ///
  /// @return Whether the click belonged to the options, and so should go no further.
  auto OnOptionPressed(const int) -> bool;
  /// @brief Takes the running side's time off its clock, and ends the game if it runs out.
  auto UpdateClocks(kuki::Application &) -> void;
  /// @brief Queues both clocks over this frame's picture.
  ///
  /// Also the turn indicator, which is the other half of what it is for: the side to move is drawn
  /// at full strength and the other dimmed. Whose move it is, is the one thing a board of
  /// primitives cannot say for itself, and saying it by colouring the pieces -- which was tried --
  /// made the pieces harder to read rather than the turn easier.
  auto DrawClocks(kuki::Application &) -> void;
  /// @brief Works out where the game stands, which is `state` and `playable`.
  ///
  /// Split from the title because the two are wanted at different moments. This is the expensive
  /// one -- `Board::State` generates every move for both sides -- and has to run before anything
  /// reads what it works out: the king takes its colour from `state`, so a redraw ahead of this
  /// draws the position as it stood a move ago. The title is cheap and has to run *after* the
  /// move it is meant to show has been recorded.
  ///
  /// It was one function doing both, named for the lesser half, and the order that suited one
  /// half was wrong for the other.
  auto UpdateStanding() -> void;
  /// @brief Puts the game's name and its recent moves in the window's title bar.
  ///
  /// Only those two. Whose move it is shows in which clock is bright, check in the `+` the
  /// notation already carries, and a fallen flag in a clock reading nothing in red -- so a
  /// running commentary here would repeat the screen rather than add to it.
  auto UpdateTitle() -> void;
};
