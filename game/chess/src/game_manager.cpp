#include <GLFW/glfw3.h>
#include <algorithm>
#include <application.hpp>
#include <array>
#include <bounding_box.hpp>
#include <camera.hpp>
#include <cmath>
#include <entity_manager.hpp>
#include <font.hpp>
#include <game_builder.hpp>
#include <game_manager.hpp>
#include <glm/ext/quaternion_float.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/trigonometric.hpp>
#include <launch_path.hpp>
#include <limits>
#include <material_asset.hpp>
#include <material_type.hpp>
#include <memory>
#include <overlay.hpp>
#include <preferences.hpp>
#include <mesh.hpp>
#include <mesh_asset.hpp>
#include <script_registry.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include <string_view>
#include <texture_asset.hpp>
#include <transform.hpp>
#include <utility>
using namespace kuki;
namespace {
/// @brief What the camera turns about, which is the middle of the board.
constexpr glm::vec3 ORBIT_CENTER{.0f, .0f, .0f};
/// @brief Radians of turn per pixel of drag.
constexpr auto ORBIT_SPEED = .005f;
/// @brief Mesh, footprint and standing height for each kind of piece.
///
/// Built from the engine's primitives rather than from models, so the game needs no art to run
/// and no art to package. Rank is conveyed by height, which is what makes a board of cylinders
/// and cubes readable at a glance.
struct PieceShape {
  const char *mesh;
  glm::vec3 scale;
};
auto ShapeOf(const PieceType type) -> PieceShape {
  switch (type) {
  case PieceType::Pawn:
    return {"Sphere", {.55f, .55f, .55f}};
  case PieceType::Knight:
    return {"Cube", {.4f, .75f, .6f}};
  case PieceType::Bishop:
    return {"Cylinder", {.5f, .95f, .5f}};
  case PieceType::Rook:
    return {"Cube", {.6f, .7f, .6f}};
  case PieceType::Queen:
    return {"Cylinder", {.62f, 1.15f, .62f}};
  case PieceType::King:
    return {"Cylinder", {.66f, 1.35f, .66f}};
  default:
    return {"Cube", {.5f, .5f, .5f}};
  }
}
/// @brief Whether an upright cylinder stands between the eye and a point.
///
/// The line of sight is clipped to the slab of height the cylinder occupies and then tested against
/// its footprint in plan, which is a ray through a disc. Two cheap tests in place of anything that
/// would need the projection: nothing here depends on where the window's edges are, so it holds
/// whatever the camera is doing.
auto Blocks(const glm::vec3 &eye, const glm::vec3 &target, const glm::vec3 &centre, const float radius, const float bottom, const float top) -> bool {
  constexpr auto EPSILON = 1e-6f;
  const auto ray = target - eye;
  auto first = .0f;
  auto last = 1.f;
  if (std::abs(ray.y) < EPSILON) {
    if (eye.y < bottom || eye.y > top)
      return false;
  } else {
    auto low = (bottom - eye.y) / ray.y;
    auto high = (top - eye.y) / ray.y;
    if (low > high)
      std::swap(low, high);
    first = std::max(first, low);
    last = std::min(last, high);
    if (first > last)
      return false;
  }
  const auto offsetX = eye.x - centre.x;
  const auto offsetZ = eye.z - centre.z;
  const auto a = ray.x * ray.x + ray.z * ray.z;
  const auto b = 2.f * (offsetX * ray.x + offsetZ * ray.z);
  const auto c = offsetX * offsetX + offsetZ * offsetZ - radius * radius;
  // A line of sight straight down the cylinder's own axis has no horizontal extent to solve for,
  // and blocks exactly when the eye is already over the footprint.
  if (a < EPSILON)
    return c <= .0f;
  const auto discriminant = b * b - 4.f * a * c;
  if (discriminant < .0f)
    return false;
  const auto root = std::sqrt(discriminant);
  return std::max(first, (-b - root) / (2.f * a)) <= std::min(last, (-b + root) / (2.f * a));
}
/// @brief Rotation that points a camera's forward at a target. Forward is -Z of the rotation.
auto LookAt(const glm::vec3 &from, const glm::vec3 &to) -> glm::quat {
  return glm::quatLookAt(glm::normalize(to - from), glm::vec3(.0f, 1.f, .0f));
}

/// @brief What each side's clock is drawn in, which is what its pieces are drawn in.
///
/// Copied from `piece_white.mat` and `piece_black.mat` rather than read out of them: the materials
/// are loaded by name and the overlay wants a colour before any of that, and a clock half a shade
/// off its pieces is not a bug anyone would notice. Worth changing here if those files change.
constexpr glm::vec3 PIECE_WHITE{.9f, .88f, .83f};
constexpr glm::vec3 PIECE_BLACK{.06f, .06f, .07f};
/// @brief What a clock turns when there is almost nothing left on it.
constexpr glm::vec3 CLOCK_URGENT{.94f, .35f, .3f};
/// @brief The option in force, the one under the pointer, and the ones merely on offer.
///
/// Hover is the active blue at less than full strength rather than a colour of its own, so that
/// pointing at an option previews what choosing it would look like instead of introducing a third
/// thing to learn.
constexpr glm::vec4 OPTION_ACTIVE{.35f, .62f, 1.f, 1.f};
constexpr glm::vec4 OPTION_HOVER{.35f, .62f, 1.f, .7f};
constexpr glm::vec4 OPTION_IDLE{1.f, 1.f, 1.f, .35f};
/// @brief Index into anything kept one per side, White first.
auto SideIndex(const PieceColor color) -> size_t {
  return color == PieceColor::White ? 0 : 1;
}
/// @brief A clock as minutes and seconds, rounded up so that it reads 0:00 only once it is spent.
auto ClockText(const float seconds) -> std::string {
  const auto total = static_cast<int>(std::ceil(std::max(seconds, .0f)));
  const auto rest = total % 60;
  return std::to_string(total / 60) + (rest < 10 ? ":0" : ":") + std::to_string(rest);
}
auto FlagText(const size_t side) -> std::string {
  return side == 0 ? "White is out of time, Black wins" : "Black is out of time, White wins";
}
auto StateText(const Board &board, const GameState state) -> std::string {
  const auto mover = board.ToMove() == PieceColor::White ? "White" : "Black";
  const auto winner = board.ToMove() == PieceColor::White ? "Black" : "White";
  switch (state) {
  case GameState::Check:
    return std::string(mover) + " to move -- check";
  case GameState::Checkmate:
    return std::string("Checkmate, ") + winner + " wins";
  case GameState::Stalemate:
    return "Stalemate -- draw";
  default:
    return std::string(mover) + " to move";
  }
}
} // namespace
GameManager::GameManager()
  : Script(std::in_place_type<GameManager>) {
  hidden.fill(-1);
}
auto GameManager::CloneTo(EntityManager &entityManager, const EntityID id) const -> void {
  auto *script = entityManager.AddComponent<GameManager>(id);
  // The board is worth copying; the entities it was drawn with belong to the original and the
  // input actions belong to whoever registered them, so both are left for `Start` to redo.
  script->board = board;
  script->squares = {};
  script->labels = {};
  script->clocks = {StartSeconds(), StartSeconds()};
  script->timeOption = timeOption;
  script->incrementOption = incrementOption;
  script->flagged = -1;
  script->pieces = {};
  script->reachable = {};
  script->capturable = {};
  script->state = GameState::Playing;
  script->hidden.fill(-1);
  script->selected = -1;
  script->playable = true;
  script->orbiting = false;
  script->pressAction = InputManager::InvalidActionID;
  script->releaseAction = InputManager::InvalidActionID;
  script->quitAction = InputManager::InvalidActionID;
  script->restartAction = InputManager::InvalidActionID;
}
auto GameManager::GetTypeName() const -> std::string {
  return "GameManager";
}
auto GameManager::SquareCenter(const int square) -> glm::vec3 {
  const auto file = static_cast<float>(Board::FileOf(square));
  const auto rank = static_cast<float>(Board::RankOf(square));
  // White's home rank sits nearest the camera, which is what makes the board read the way a player
  // expects rather than mirrored.
  return {(file - 3.5f) * SQUARE_SIZE, .0f, (3.5f - rank) * SQUARE_SIZE};
}
auto GameManager::Start(Application &application) -> void {
  auto *app = GetApp();
  if (!app)
    return;
  LoadSettings(application);
  clocks = {StartSeconds(), StartSeconds()};
  CreateLabels();
  // Before `Refresh`, which draws the pieces and reads the king's colour off `state`.
  UpdateTitle();
  Refresh();
  pressAction = app->RegisterInputAction(GLFW_MOUSE_BUTTON_LEFT, [this]() { OnPress(); });
  releaseAction = app->RegisterInputAction(
    GLFW_MOUSE_BUTTON_LEFT, [this]() { orbiting = false; }, false);
  quitAction = app->RegisterInputAction(GLFW_KEY_ESCAPE, [this]() {
    // Through the base rather than captured: an input action outlives the call that registered it.
    if (auto *self = GetApp(); self)
      self->Quit();
  });
  restartAction = app->RegisterInputAction(GLFW_KEY_R, [this]() { Restart(); });
  spdlog::info("[Chess] Click a piece to select it, then a green square to move it; red marks a piece it can take; drag anywhere else to turn the board; R restarts, Escape quits");
}
auto GameManager::Update(Application &application) -> void {
  if (orbiting)
    Orbit(application);
  UpdateClocks(application);
  DrawClocks(application);
  DrawOptions(application);
  // Which pieces are in the way depends on where the camera is and not only on what is selected, so
  // it is re-checked every frame: turning the board takes a piece out of the way as surely as
  // taking it does. Nothing is redrawn unless the answer has actually changed.
  if (UpdateOcclusion())
    RefreshPieces();
}
auto GameManager::StartSeconds() const -> float {
  return static_cast<float>(TIME_OPTIONS[timeOption]) * 60.f;
}
auto GameManager::IncrementSeconds() const -> float {
  return static_cast<float>(INCREMENT_OPTIONS[incrementOption]);
}
auto GameManager::LoadSettings(Application &application) -> void {
  const auto &preferences = application.GetPreferences();
  // Matched by value rather than stored as an index, so that adding a control to either list --
  // or reordering one -- leaves a saved choice meaning what it meant when it was saved.
  const auto minutes = preferences.GetInt(PREF_MINUTES, TIME_OPTIONS[timeOption]);
  const auto increment = preferences.GetInt(PREF_INCREMENT, INCREMENT_OPTIONS[incrementOption]);
  for (size_t i = 0; i < std::size(TIME_OPTIONS); ++i)
    if (TIME_OPTIONS[i] == minutes)
      timeOption = i;
  for (size_t i = 0; i < std::size(INCREMENT_OPTIONS); ++i)
    if (INCREMENT_OPTIONS[i] == increment)
      incrementOption = i;
}
auto GameManager::SaveSettings(Application &application) -> void {
  auto &preferences = application.GetPreferences();
  preferences.Set(PREF_MINUTES, TIME_OPTIONS[timeOption]);
  preferences.Set(PREF_INCREMENT, INCREMENT_OPTIONS[incrementOption]);
}
auto GameManager::DrawOptions(Application &application) -> void {
  auto &overlay = application.GetOverlay();
  const auto &font = overlay.GetFont();
  if (!font.IsLoaded())
    return;
  // Asked once for the whole overlay rather than once an option, and answered from where the last
  // frame put things -- which is what the pointer is actually over, since that is the frame on
  // screen while it is being pointed at.
  const auto hovered = application.PickOverlay(application.GetMousePosition());
  const auto Row = [&](const auto &options, const size_t chosen, const int idBase, const char *suffix, const bool plus, const float y) {
    auto x = OPTION_MARGIN;
    for (size_t i = 0; i < std::size(options); ++i) {
      const auto text = (plus ? "+" : "") + std::to_string(options[i]) + suffix;
      // Stepped by the ink rather than by the advance, because the ink is what the anchor places:
      // stepping by anything else leaves the gaps between the options visibly uneven.
      glm::vec2 min, max;
      if (!font.Bounds(text, min, max))
        continue;
      const auto id = idBase + static_cast<int>(i);
      const auto tint = i == chosen ? OPTION_ACTIVE : id == hovered ? OPTION_HOVER
                                                                    : OPTION_IDLE;
      overlay.DrawText(text, {x, y}, OPTION_TEXT_SIZE, tint, TextAnchor::TopLeft, id);
      x += (max.x - min.x) * OPTION_TEXT_SIZE + OPTION_SPACING;
    }
  };
  // Minutes above, increment below, both in the top left corner. The units are on every figure --
  // "5m", "+2s" -- so neither row needs a label to say what it is, which is a row of words saved
  // on a screen that is mostly board.
  Row(TIME_OPTIONS, timeOption, TIME_OPTION_ID, "m", false, OPTION_MARGIN);
  Row(INCREMENT_OPTIONS, incrementOption, INCREMENT_OPTION_ID, "s", true, OPTION_MARGIN + OPTION_ROW_STEP);
}
auto GameManager::OnOptionPressed(const int id) -> bool {
  const auto Choose = [&](const int base, const size_t count, size_t &chosen) {
    const auto index = static_cast<size_t>(id - base);
    if (id < base || index >= count)
      return false;
    // A click on the option already in force is not a change, and so is not a reason to throw away
    // the game being played. It still counts as handled: the click was on the options.
    if (index != chosen) {
      chosen = index;
      if (auto *app = GetApp(); app)
        SaveSettings(*app);
      Restart();
    }
    return true;
  };
  return Choose(TIME_OPTION_ID, std::size(TIME_OPTIONS), timeOption) || Choose(INCREMENT_OPTION_ID, std::size(INCREMENT_OPTIONS), incrementOption);
}
auto GameManager::UpdateClocks(Application &application) -> void {
  if (!playable)
    return;
  const auto side = SideIndex(board.ToMove());
  clocks[side] = std::max(clocks[side] - application.DeltaTime(), .0f);
  if (clocks[side] > .0f)
    return;
  flagged = static_cast<int>(side);
  // Whatever was in hand is put down: the game is over, and a piece left lifted would go on
  // offering moves that can no longer be played.
  Select(-1);
  UpdateTitle();
  spdlog::info("[Chess] {}", FlagText(side));
}
auto GameManager::DrawClocks(Application &application) -> void {
  auto &overlay = application.GetOverlay();
  const auto Draw = [&](const PieceColor color, const float y) {
    const auto side = SideIndex(color);
    const auto remaining = clocks[side];
    const auto running = playable && color == board.ToMove();
    // The side's own colour, so a clock needs no label to say whose it is: the light one belongs to
    // the light pieces. Red is the one thing allowed to override that, because a clock about to run
    // out has something to say that its owner's colour cannot.
    const auto own = color == PieceColor::White ? PIECE_WHITE : PIECE_BLACK;
    const auto tint = remaining <= CLOCK_LOW ? CLOCK_URGENT : own;
    // Faded once it is not this side's move, which is the turn indicator. Not faded to nothing --
    // both clocks stay readable, and it is the pair being unequal that says whose move it is.
    // The idle clock is dimmed rather than half dissolved. It has to stay readable -- both times
    // are worth knowing at a glance, and it is the pair being unequal that says whose move it is,
    // which a smaller difference carries just as well as a large one.
    const glm::vec4 shade{tint.x, tint.y, tint.z, running ? 1.f : .7f};
    // Right-anchored, so the digits hold still where they are read rather than sliding left as the
    // minutes fall from two figures to one.
    overlay.DrawText(ClockText(remaining), {-CLOCK_MARGIN, y}, CLOCK_TEXT_SIZE, shade, TextAnchor::Right);
  };
  // Facing one another across the middle of the right edge, Black above and White below -- the
  // order they sit in on the board from where the camera starts, so a player finds their own clock
  // at their own end of it.
  Draw(PieceColor::Black, -CLOCK_GAP);
  Draw(PieceColor::White, CLOCK_GAP);
}
auto GameManager::Orbit(Application &application) -> void {
  auto *camera = application.GetCamera();
  if (!camera) {
    orbiting = false;
    return;
  }
  // Never quite level with the board and never straight down: the board is a flat slab, so a view
  // from underneath shows nothing, and the pole is where a look-at with a fixed world up gives out.
  static constexpr auto MIN_PITCH = glm::radians(6.f);
  static constexpr auto MAX_PITCH = glm::radians(88.f);
  const auto mouse = application.GetMousePosition();
  const auto drag = mouse - orbitLast;
  orbitLast = mouse;
  if (drag.x == .0f && drag.y == .0f)
    return;
  // Dragging right turns the board right, which means the camera goes the other way; dragging up
  // brings the far edge up, which lowers the camera. Both are the sense of taking hold of the board
  // rather than of the camera, and both match the editor's own orbit.
  orbitYaw -= drag.x * ORBIT_SPEED;
  orbitPitch = std::clamp(orbitPitch + drag.y * ORBIT_SPEED, MIN_PITCH, MAX_PITCH);
  const auto cosPitch = std::cos(orbitPitch);
  camera->position = ORBIT_CENTER + orbitDistance * glm::vec3(cosPitch * std::sin(orbitYaw), std::sin(orbitPitch), cosPitch * std::cos(orbitYaw));
  camera->rotation = LookAt(camera->position, ORBIT_CENTER);
  // The basis, the view matrix and the frustum are all derived from those two and none of them
  // recomputes itself; the counter is what tells the renderer the camera it uploaded is stale.
  camera->Update();
  ++camera->dirty;
}
auto GameManager::Shutdown(Application &application) -> void {
  // Written on the way out as well as on every change, so that a first run leaves a file behind
  // rather than only the runs that touched a setting.
  SaveSettings(application);
  // Unregistered explicitly, because the actions capture `this` and the input manager outlives the
  // scene: a stale action would call into a destroyed script on the next click.
  for (const auto action : {pressAction, releaseAction, quitAction, restartAction})
    if (action != InputManager::InvalidActionID)
      application.UnregisterInputAction(action);
  pressAction = InputManager::InvalidActionID;
  releaseAction = InputManager::InvalidActionID;
  quitAction = InputManager::InvalidActionID;
  restartAction = InputManager::InvalidActionID;
}
auto GameManager::Refresh() -> void {
  RefreshSquares();
  RefreshPieces();
}
auto GameManager::RefreshSquares() -> void {
  auto *app = GetApp();
  if (!app)
    return;
  auto builder = app->Game().Scene("Main");
  for (auto square = 0; square < Board::SquareCount; ++square) {
    const auto light = (Board::FileOf(square) + Board::RankOf(square)) % 2 == 1;
    const auto center = SquareCenter(square);
    // The whole of the legal-move display. Two materials per colour rather than one, because each
    // is the square's own shade with green or red mixed into it: a flat green over both shades
    // would say where the piece may go and take the board's pattern away while it said it, which
    // is most of what tells one square from the next.
    //
    // Red before green, because a square that can be moved to by taking what stands on it has more
    // to say about the piece than about the square -- and for every capture but en passant the two
    // are the same square, so one of them has to win.
    const auto *material = capturable[square] ? (light ? "square_light_capture" : "square_dark_capture")
      : reachable[square]                     ? (light ? "square_light_move" : "square_dark_move")
                                              : (light ? "square_light" : "square_dark");
    // Full width, so a square meets its neighbours rather than floating in a grid of gaps. The
    // slab is positioned by its upper face, since that is the surface everything else stands on.
    squares[square] = (squares[square] ? builder.Entity(squares[square]) : builder.Entity("square" + std::to_string(square)))
                        .Mesh("Cube")
                        .Material(material)
                        .Scale({SQUARE_SIZE, SQUARE_HEIGHT, SQUARE_SIZE})
                        .At({center.x, -SQUARE_HEIGHT * .5f, center.z})
                        .Id();
  }
}
auto GameManager::CreateLabels() -> void {
  auto *app = GetApp();
  if (!app || labels)
    return;
  // The overlay's font and the overlay's atlas, rather than a bake of this game's own. `Chess`
  // sets it before the scripts start, and one atlas can serve both: what the labels need from it
  // is where the glyphs are, which does not depend on whether they are going onto the board or
  // onto the window.
  const auto &font = app->GetOverlay().GetFont();
  const auto atlasId = app->GetOverlay().GetAtlasAssetId();
  if (!font.IsLoaded() || !atlasId)
    return;
  const auto materialId = MakeBuiltInAssetID("chess/label");
  auto materialAsset = std::make_unique<MaterialAsset>(materialId);
  materialAsset->type = MaterialType::Lit;
  // Off-white and matt, so the labels read as painted onto the table rather than as another piece.
  materialAsset->fallback.albedo = {.86f, .84f, .79f, 1.f};
  materialAsset->fallback.metalness = .0f;
  materialAsset->fallback.roughness = .9f;
  // A cutout rather than a blend. The atlas is white everywhere with the glyph in its alpha, so
  // what is wanted is the ink and nothing else -- and a cutout writes depth and needs no sorting,
  // where a blended quad lying on the ground would have to be drawn after everything opaque.
  materialAsset->fallback.alphaMode = AlphaMode::Mask;
  materialAsset->fallback.alphaCutoff = .5f;
  materialAsset->textures.push_back(atlasId);
  app->AddAsset(std::move(materialAsset), "label");
  // Half the board's width, which is where the squares stop and the labels begin.
  constexpr auto EDGE = Board::Size * SQUARE_SIZE * .5f;
  constexpr auto BAND = EDGE + LABEL_MARGIN + LABEL_SIZE * .5f;
  Mesh mesh;
  // Laid out in ems and scaled to world units by the entity, so world measurements are divided on
  // the way in. `y` runs opposite to world `z`, which is what the quarter turn about x below makes
  // of it, and each label is placed by the middle of its ink rather than by its pen: "8" and "a"
  // carry different amounts of it, and lining up the pens leaves them looking misaligned.
  const auto Place = [&](const std::string_view text, const float x, const float z) {
    glm::vec2 min, max;
    if (!font.Bounds(text, min, max))
      return;
    const glm::vec2 center{x / LABEL_SIZE, -z / LABEL_SIZE};
    font.Append(text, mesh, center - (min + max) * .5f);
  };
  for (auto file = 0; file < Board::Size; ++file)
    Place(std::string(1, static_cast<char>('a' + file)), SquareCenter(Board::SquareOf(file, 0)).x, BAND);
  for (auto rank = 0; rank < Board::Size; ++rank)
    Place(std::to_string(rank + 1), -BAND, SquareCenter(Board::SquareOf(0, rank)).z);
  auto meshAsset = std::make_unique<MeshAsset>(MakeBuiltInAssetID("chess/labels"));
  meshAsset->material = materialId;
  meshAsset->bounds = BoundingBox::Calculate(mesh.vertices);
  meshAsset->mesh = std::move(mesh);
  app->AddAsset(std::move(meshAsset), "labels");
  labels = app->Game()
             .Scene("Main")
             .Entity("Labels")
             .Mesh("labels")
             .Material("label")
             .Scale(LABEL_SIZE)
             // Level with the board's surface, and a quarter turn about x to lay the
             // glyphs face-up: that turn takes the mesh's +z normal to +y and its +y to -z, so the
             // text both faces the sky and reads from White's side of the board.
             .At({.0f, .0f, .0f})
             .RotateEuler({-90.f, .0f, .0f})
             .Id();
}
auto GameManager::RefreshPieces() -> void {
  auto *app = GetApp();
  if (!app)
    return;
  auto builder = app->Game().Scene("Main");
  for (auto square = 0; square < Board::SquareCount; ++square) {
    const auto piece = board.Get(square);
    if (!piece.Occupied()) {
      if (pieces[square])
        app->DeleteEntity(pieces[square]);
      pieces[square] = {};
      continue;
    }
    const auto shape = ShapeOf(piece.type);
    const auto center = SquareCenter(square);
    const auto lift = selected == square ? SELECT_LIFT : .0f;
    const auto white = piece.color == PieceColor::White;
    // The king of the side to move is the only piece that says anything about itself, and it is the
    // only one with anything left to say: everything about a move is on the squares now.
    const auto king = piece.type == PieceType::King && piece.color == board.ToMove();
    // Check before the ghost, because a king in check is worth seeing through a piece standing in
    // front of it rather than instead of it.
    const auto *material = king && state == GameState::Checkmate ? "piece_checkmate"
      : king && state == GameState::Check                        ? "piece_check"
      : hidden[square] >= 0                                      ? (white ? "piece_white_ghost" : "piece_black_ghost")
                                                                 : (white ? "piece_white" : "piece_black");
    // Every value below is written whether the entity is new or reused. A square a knight has left
    // and a queen has arrived on keeps one entity, and anything not overwritten here would still be
    // the knight's -- so the rotation is set on all of them rather than only where it is wanted.
    pieces[square] = (pieces[square] ? builder.Entity(pieces[square]) : builder.Entity("piece" + std::to_string(square)))
                       .Mesh(shape.mesh)
                       .Material(material)
                       .Scale(shape.scale)
                       // Standing on the board's surface, which is what the slabs' upper faces are.
                       .At({center.x, shape.scale.y * .5f + lift, center.z})
                       // A knight is turned off-axis so its cube is not mistaken for a rook's.
                       .RotateEuler({.0f, piece.type == PieceType::Knight ? 25.f : .0f, .0f})
                       .Id();
  }
}
auto GameManager::Select(const int square) -> void {
  selected = square;
  reachable = {};
  capturable = {};
  if (square >= 0)
    for (const auto &move : board.LegalMoves(square)) {
      // Where the piece may go, which is what a click has to land on to make the move.
      reachable[move.to] = true;
      // The square the taken piece is standing on, which is not the destination for en passant.
      // Marking `to` instead would leave the pawn that is actually in danger looking safe and
      // colour an empty square that no piece is standing on.
      if (move.captured >= 0)
        capturable[move.captured] = true;
    }
  UpdateOcclusion();
  Refresh();
}
auto GameManager::UpdateOcclusion() -> bool {
  std::array<int, Board::SquareCount> next;
  next.fill(-1);
  auto *app = GetApp();
  const auto *camera = app ? app->GetCamera() : nullptr;
  if (camera && playable) {
    // What the next click is trying to reach, and the point on it a click would be aimed at. With a
    // piece selected that is the top of each square it may move to; with none it is this side's own
    // pieces, since one of those hidden behind an enemy piece cannot be picked up at all.
    std::array<glm::vec3, Board::SquareCount> aim{};
    std::array<bool, Board::SquareCount> wanted{};
    for (auto target = 0; target < Board::SquareCount; ++target) {
      const auto centre = SquareCenter(target);
      if (selected >= 0) {
        // The destinations only. The square an en passant capture empties is red but is not
        // somewhere the piece can be put, so nothing is aimed at it.
        if (!reachable[target])
          continue;
        wanted[target] = true;
        aim[target] = centre;
      } else {
        const auto piece = board.Get(target);
        if (!piece.Occupied() || piece.color != board.ToMove())
          continue;
        wanted[target] = true;
        // The middle of the piece rather than the square under it, because the piece is what a
        // click picking it up would be aimed at.
        aim[target] = {centre.x, ShapeOf(piece.type).scale.y * .5f, centre.z};
      }
    }
    for (auto square = 0; square < Board::SquareCount; ++square) {
      const auto piece = board.Get(square);
      // Only the opponent's, and only the ones a click has nothing to say to. One of this side's
      // pieces is worth clicking on its own account, and one that can be taken is a destination.
      if (!piece.Occupied() || piece.color == board.ToMove() || capturable[square])
        continue;
      const auto shape = ShapeOf(piece.type);
      const auto standing = SquareCenter(square);
      const auto radius = std::max(shape.scale.x, shape.scale.z) * .5f;
      auto nearest = std::numeric_limits<float>::max();
      for (auto target = 0; target < Board::SquareCount; ++target) {
        if (!wanted[target] || !Blocks(camera->position, aim[target], standing, radius, .0f, shape.scale.y))
          continue;
        // The nearest of them, because that is the one a click aimed through this piece would have
        // reached first if the piece were not there.
        if (const auto distance = glm::length(aim[target] - camera->position); distance < nearest) {
          nearest = distance;
          next[square] = target;
        }
      }
    }
  }
  if (next == hidden)
    return false;
  hidden = next;
  return true;
}
auto GameManager::SquareOfEntity(const EntityID id) const -> int {
  if (!id)
    return -1;
  for (auto square = 0; square < Board::SquareCount; ++square)
    if (pieces[square] == id || squares[square] == id)
      return square;
  return -1;
}
auto GameManager::OnPress() -> void {
  auto *app = GetApp();
  if (!app)
    return;
  // The overlay first, because it is drawn over everything and a click that lands on a caption was
  // aimed at the caption rather than at whatever the caption is covering.
  if (OnOptionPressed(app->PickOverlay(app->GetMousePosition())))
    return;
  const auto picked = app->PickEntity(app->GetMousePosition());
  auto square = SquareOfEntity(picked);
  // A see-through piece does not take the click. It is drawn that way to say the square behind it
  // is reachable, and handing the click on is the other half of saying so -- tested against the
  // piece rather than the square, so the slab it stands on is still its own thing to click.
  if (square >= 0 && pieces[square] == picked && hidden[square] >= 0)
    square = hidden[square];
  if (playable && square >= 0) {
    if (selected >= 0)
      if (const auto move = board.Legal(selected, square); move) {
        // Read before the move, since applying it is what changes whose turn it is. The increment
        // goes to the side that has just moved, which is how an increment works: it pays for the
        // move that was made rather than for the one about to be.
        const auto mover = SideIndex(board.ToMove());
        board.Apply(*move);
        clocks[mover] += IncrementSeconds();
        // Before `Select`, which redraws the pieces and reads the king's colour off `state`.
        UpdateTitle();
        Select(-1);
        return;
      }
    // Not a move, so it is a new selection if it is one of this side's pieces. Selecting only those
    // is what keeps a player from dragging their opponent's pieces about.
    const auto piece = board.Get(square);
    if (piece.Occupied() && piece.color == board.ToMove()) {
      Select(square);
      return;
    }
  }
  // Nothing on the board answers this press, so the camera takes it -- and the selection stays put.
  // Turning the board to look at a move from another angle is not a change of mind about the piece,
  // and the coloured squares are most of what there is to look at while turning it.
  BeginOrbit();
}
auto GameManager::BeginOrbit() -> void {
  auto *app = GetApp();
  const auto *camera = app ? app->GetCamera() : nullptr;
  if (!camera)
    return;
  const auto offset = camera->position - ORBIT_CENTER;
  orbitDistance = glm::length(offset);
  // A camera sitting on the point it turns about has no angles to take, and nothing sensible to do
  // with a drag either.
  if (orbitDistance <= .0f)
    return;
  orbitPitch = std::asin(std::clamp(offset.y / orbitDistance, -1.f, 1.f));
  orbitYaw = std::atan2(offset.x, offset.z);
  orbitLast = app->GetMousePosition();
  orbiting = true;
}
auto GameManager::Restart() -> void {
  board.Reset();
  clocks = {StartSeconds(), StartSeconds()};
  flagged = -1;
  // Before `Select` for the reason given there: the pieces are drawn from `state`, and a new game
  // whose standing still said checkmate would open with a red king.
  UpdateTitle();
  Select(-1);
  spdlog::info("[Chess] New game");
}
auto GameManager::UpdateTitle() -> void {
  state = board.State();
  playable = flagged < 0 && state != GameState::Checkmate && state != GameState::Stalemate;
  if (auto *app = GetApp(); app)
    app->SetWindowTitle("Chess -- " + (flagged >= 0 ? FlagText(static_cast<size_t>(flagged)) : StateText(board, state)));
}
KUKI_REGISTER_SCRIPT(GameManager)
