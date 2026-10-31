#pragma once
#include <array>
#include <board.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <id.hpp>
#include <input_manager.hpp>
#include <string>
#include <string_view>
#include <system.hpp>
namespace kuki {
class Application;
class EntityManager;
} // namespace kuki
/// @brief Everything the chess game does that is not a rule: geometry, picking, and turn flow.
///
/// A system: there is one game in the application and this is it, so it holds the board itself
/// rather than a component on some entity doing so. It was a script on an entity of its own, which
/// was the engine's one extension point for game code at the time, and nothing about it was ever
/// that entity's.
class GameManager final : public kuki::System {
public:
  explicit GameManager(kuki::Application &);
  /// @brief Draws the board, the clock and the labels, takes the mouse, and puts the game's own
  /// settings in the settings menu. After the scene's skeleton has been built, and the font baked
  /// whose atlas the labels are drawn from.
  auto PostStart() -> void override;
  auto Update(const float) -> void override;
  auto Shutdown() -> void override;
private:
  auto Begin(kuki::Application &) -> void;
  auto Frame(kuki::Application &) -> void;
  auto End(kuki::Application &) -> void;
  /// @brief The application, for the code written when this was a script and asked for it.
  auto GetApp() -> kuki::Application * {
    return &app;
  }
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
  /// @brief Key the chosen board theme is kept under, by the theme's key rather than its place in
  /// the list, for the same reason the time control is kept by value.
  static constexpr const char *PREF_THEME = "board.theme";
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
  /// @brief How far the labels sit above the board frame's top face, which is also the squares'.
  ///
  /// The labels are painted onto the frame, and two surfaces at the same height fight over which
  /// one the depth test shows. This is far more than the depth buffer can resolve at the distances
  /// the camera works at and far less than anyone can see.
  static constexpr float LABEL_LIFT = .004f;
  /// @brief Where the clock stands: to White's right, beside the board, on the plane the board's
  /// foot rests on.
  ///
  /// The model's long axis is its x and its face looks along its +z, tilted up. A quarter turn
  /// about y puts the long axis along the board's z -- White's half of the clock at White's end --
  /// and turns the face in toward the board, which is the side the camera looks from. Facing out,
  /// as a clock at a tournament does, would show the camera its back.
  static constexpr glm::vec3 CLOCK_POSITION{6.f, -.39f, .0f};
  static constexpr float CLOCK_YAW = -90.f;
  /// @brief Where each screen's glass is in the clock model's own space, and how it leans.
  ///
  /// Read off the model rather than worked out here: these are the numbers Blender printed when
  /// the clock was built, and they have to change with it. White's screen is the one at +x, which
  /// the turn above puts at White's end. The glyphs sit a few thousandths in front of the glass
  /// for the same reason the labels sit above the frame.
  ///
  /// The clock is drawn minimal rather than as any real model: a plain body, a rocker, and two
  /// screens that take up most of its face, one a side with a slim gap between them.
  static constexpr float SCREEN_X = .8f;
  static constexpr float SCREEN_Y = .6032f;
  static constexpr float SCREEN_Z = .4043f;
  static constexpr float SCREEN_TILT = -22.f;
  /// @brief How much of each screen's 1.46 width the widest reading, `00:00`, is allowed to fill.
  ///
  /// The glyph size is worked out from this and the font's own advances rather than set, so the
  /// figures fill the glass however wide the typeface draws its digits.
  static constexpr float SCREEN_TEXT_WIDTH = 1.16f;
  /// @brief Where the rocker turns in the clock model's space, and how far it leans either way.
  ///
  /// The rocker is the one bar across the clock's top that both players press: whichever end is
  /// down belongs to the player who pressed it, and so to the player who is waiting. Its top dips
  /// to the middle, so the end that is down lies near level, as a real one does.
  static constexpr glm::vec3 ROCKER_PIVOT{.0f, .97f, -.17f};
  static constexpr float ROCKER_TILT = 7.f;
  /// @brief How quickly the rocker swings to where it is going, as a rate of exponential approach.
  ///
  /// Fast enough to read as a press -- most of the way over in about a tenth of a second -- and
  /// not so fast that it jumps, which would look like the model being swapped rather than moved.
  static constexpr float ROCKER_SPEED = 22.f;
  /// @brief Seconds remaining below which a clock turns red.
  static constexpr float CLOCK_LOW = 30.f;
  /// @brief How long a capturing piece waits, lifted over the square it left, before it lands.
  ///
  /// The taken piece comes apart where it stands, and for the first part of that it is still
  /// standing there: its grains are held in place and let go a few at a time. A piece set down on
  /// the square in the same frame would be set down inside it. This is a little longer than the
  /// last grain is held for, so the square has begun to clear by the time anything arrives on it.
  static constexpr float ARRIVE_DELAY = .4f;
  /// @brief The longest a grain of a taken piece is held, and the longest it then lasts, in seconds.
  ///
  /// Under a second between them, which is the whole of a capture from the cut to the last grain:
  /// a move is one beat of the game, and an effect that outlasts it is still playing when the reply
  /// is being thought about. The sparks are held to the same second.
  static constexpr float DISSOLVE_HOLD = .3f;
  static constexpr float DISSOLVE_LIFETIME = .65f;
  /// @brief How long the slash takes to cross a taken piece, in seconds, and the rate its emitter
  /// runs at while it does.
  ///
  /// The slash is an emitter swept along a line. The renderer spreads a frame's spawns along the
  /// path the emitter travelled in that frame, so what the sweep leaves behind is a line of sparks
  /// whose density is this rate over the speed of the sweep -- seven hundred along a line a little
  /// longer than a piece is tall, which is close enough together to read as one stroke. Quick
  /// enough to be a cut, and still several frames, so the stroke is seen being drawn.
  static constexpr float SLASH_SWEEP = .07f;
  static constexpr float SLASH_RATE = 10000.f;
  Board board;
  /// @brief The slab drawn for each square. Created once, and after that only ever moved.
  std::array<kuki::EntityID, Board::SquareCount> squares{};
  /// @brief Every rank and file label, as one entity. Created once and never touched again.
  kuki::EntityID labels{};
  /// @brief The frame the squares sit in. Created once and never touched again.
  kuki::EntityID frame{};
  /// @brief Character slots on one of the clock's screens, left to right: tens and units of
  /// minutes, the colon, tens and units of seconds.
  ///
  /// One entity a character rather than one a reading, because a mesh is uploaded once and never
  /// again: a reading baked into a mesh of its own would be a new asset every second. Each slot
  /// instead swaps between eleven glyph meshes made once at startup -- the same move a square makes
  /// when its material changes. A slot with nothing to show, which is the tens of minutes below
  /// ten, has no entity at all rather than an empty mesh.
  static constexpr size_t CLOCK_SLOTS = 5;
  struct ClockFace {
    std::array<kuki::EntityID, CLOCK_SLOTS> slots{};
    /// @brief What each slot is showing, so a slot is only written when its reading changes.
    std::array<char, CLOCK_SLOTS> shown{};
    /// @brief The material the whole face is lit with, likewise.
    std::string_view material{};
  };
  /// @brief The clock's root, which places it; everything else on it is a child.
  kuki::EntityID clock{};
  /// @brief One face a side, White first.
  std::array<ClockFace, 2> faces{};
  /// @brief The parts of the clock the theme colours, which is all of it but the figures.
  kuki::EntityID clockBody{};
  kuki::EntityID clockScreen{};
  kuki::EntityID rocker{};
  /// @brief The rocker's current lean in degrees about the model's z, travelling toward
  /// `ROCKER_TILT` one way or the other. Positive lifts White's end.
  float rockerAngle{};
  /// @brief The four particle emitters the game has: one swept across a piece as it is taken, one
  /// the piece is turned into, one fired where it stood, and one left running on the winner's king.
  /// Created once; after that only ever moved, switched and fired.
  ///
  /// One emitter for every capture rather than one a capture. Its particles are in world space and
  /// stop following it the moment they leave, so moving it to the next capture takes nothing away
  /// from the last one's sparks still in the air -- and an emitter made per capture would be a
  /// buffer allocated per capture, for a pool that is empty again within a second.
  kuki::EntityID slashSparks{};
  kuki::EntityID dissolveGrains{};
  kuki::EntityID captureSparks{};
  kuki::EntityID victorySparks{};
  /// @brief The square a capturing piece left and the one it is on its way to, while it waits to
  /// land, and how much longer it waits. Both -1 when nothing is in the air.
  ///
  /// The board has already made the move. This is only where the piece is drawn in the meantime:
  /// `RefreshPieces` reads it and shows the piece over its old square and nothing on its new one.
  int arriveFrom{-1};
  int arriveTo{-1};
  float arriveLeft{};
  /// @brief Whether a slash is under way, how far through its sweep it is, and the line it follows.
  ///
  /// The time is negative for a slash that has been asked for and not yet begun; see `UpdateSparks`
  /// for why there is such a state. `slashBurst` is where the burst goes off once the cut is made.
  bool slashing{};
  float slashTime{};
  glm::vec3 slashFrom{};
  glm::vec3 slashTo{};
  glm::vec3 slashBurst{};
  /// @brief Each slot's centre along the screen, and the glyphs' em, in clock units. Worked out
  /// with the glyphs, from the font's advances.
  std::array<float, CLOCK_SLOTS> slotOffsets{};
  float glyphSize{};
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
  /// @brief Which of the board themes is in force. See `THEMES` in the source.
  size_t themeOption{};
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
  /// @brief The binding that starts a new game, R unless the player has rebound it.
  static constexpr const char *RESTART_BINDING = "Chess.Restart";
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
  /// @brief Takes the running side's time off its clock, and ends the game if it runs out.
  auto UpdateClocks(kuki::Application &) -> void;
  /// @brief Lays the board's frame under the squares. Created once; recoloured by `ApplyTheme`.
  auto CreateFrame() -> void;
  /// @brief Registers every theme's square and frame materials, so switching is a renaming.
  ///
  /// Made in code rather than staged as `.mat` files because each theme needs seven of them and
  /// six are worked out from the other two: a square showing a move or a capture is its own shade
  /// mixed toward green or red, so stating a theme is stating its two shades and its frame. The
  /// clock is made of the same three colours, so its materials are made here too.
  auto CreateThemeMaterials() -> void;
  /// @brief Puts the chosen theme on the squares, the frame and the clock. The game carries on.
  auto ApplyTheme() -> void;
  /// @brief Lists the board's look, the time control and a new game in the settings menu.
  ///
  /// Where the time control and the board's look were drawn along the top of the window for the
  /// whole of every game; the menu holds them now, and the window holds the board.
  auto AddSettings(kuki::Application &) -> void;
  /// @brief Builds the clock: body, screens, rocker, and the glyph meshes the screens show.
  ///
  /// The glyphs are cut from the same atlas as the labels, one character to a mesh and each
  /// centred on its own cell, so a slot can show any of them without being moved.
  auto CreateClock() -> void;
  /// @brief Brings the clock in line with the game: the two readings and the rocker.
  ///
  /// Also the turn indicator, which is the other half of what it is for. The side to move has its
  /// figures drawn black and the other's greyed, and the rocker is down at its opponent's end --
  /// a player presses their own end to stop their clock and start the other, so the end that is
  /// down belongs to whoever is waiting. The clock presses itself: making the move is the press.
  auto UpdateClock(kuki::Application &) -> void;
  /// @brief Makes the four emitters, all stopped.
  ///
  /// They do nothing on a backend with no particle pass -- the component is there and nothing reads
  /// it -- so nothing here asks which backend is running.
  auto CreateSparks() -> void;
  /// @brief Throws a burst of sparks from a point, which is where a taken piece was standing.
  auto Burst(const glm::vec3 &) -> void;
  /// @brief Turns a piece standing on a square into grains the shape of it, which then drift apart.
  ///
  /// The piece's own entity is not involved and its mesh is not touched. The grains are an emitter
  /// that is given the piece's mesh and placed exactly where the piece stood, in the frame the
  /// piece itself stops being drawn.
  auto Dissolve(const int, const Piece &) -> void;
  /// @brief Cuts across a piece standing at a point and of a given height, then bursts from its
  /// middle.
  ///
  /// The line runs from above one shoulder to the foot on the other side, across the camera's view
  /// rather than along any axis of the board, so it is a diagonal from wherever it is watched.
  auto Slash(const glm::vec3 &, const float) -> void;
  /// @brief Carries the slash along its line, and lands a capturing piece once its wait is over.
  auto UpdateSparks(kuki::Application &) -> void;
  /// @brief Starts the victory emitter on the winner's king, or stops it when nobody has won.
  auto UpdateVictory() -> void;
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
  /// Only those two. Whose move it is shows in which time is drawn black and which way the rocker
  /// leans, check in the `+` the notation already carries, and a fallen flag in a time reading
  /// nothing in red -- so a running commentary here would repeat the scene rather than add to it.
  auto UpdateTitle() -> void;
};
