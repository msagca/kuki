#include <GLFW/glfw3.h>
#include <algorithm>
#include <application.hpp>
#include <array>
#include <camera.hpp>
#include <cmath>
#include <entity_manager.hpp>
#include <game_builder.hpp>
#include <game_manager.hpp>
#include <glm/ext/quaternion_float.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/trigonometric.hpp>
#include <limits>
#include <script_registry.hpp>
#include <spdlog/spdlog.h>
#include <string>
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
  script->pieces = {};
  script->rise = {};
  script->capturable = {};
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
auto GameManager::SquareTop(const int square) const -> float {
  return rise[square];
}
auto GameManager::Start(Application &application) -> void {
  auto *app = GetApp();
  if (!app)
    return;
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
  UpdateTitle();
  spdlog::info("[Chess] click a piece to select it, then a raised square to move it; red pieces can be taken; drag anywhere else to turn the board; R restarts, Escape quits");
}
auto GameManager::Update(Application &application) -> void {
  if (orbiting)
    Orbit(application);
  // Which pieces are in the way depends on where the camera is and not only on what is selected, so
  // it is re-checked every frame: turning the board takes a piece out of the way as surely as
  // taking it does. Nothing is redrawn unless the answer has actually changed.
  if (UpdateOcclusion())
    RefreshPieces();
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
    // Full width, so a square meets its neighbours rather than floating in a grid of gaps. The
    // slab is positioned by its upper face, since that is the height everything else is measured
    // from and the only part of it a raised square is meant to say anything with.
    squares[square] = (squares[square] ? builder.Entity(squares[square]) : builder.Entity("square" + std::to_string(square)))
                        .Mesh("Cube")
                        .Material(light ? "square_light" : "square_dark")
                        .Scale({SQUARE_SIZE, SQUARE_HEIGHT, SQUARE_SIZE})
                        .At({center.x, SquareTop(square) - SQUARE_HEIGHT * .5f, center.z})
                        .Id();
  }
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
    // Red where the selection may take it, see-through where it is merely standing in front of
    // somewhere the selection may go, greyed towards the board where it is simply not this side's
    // turn, and its own colour when it is.
    //
    // The dimmed pair are both nearer a mid grey rather than both darker, because darkening a piece
    // that is already very nearly black says nothing: what reads as out of play is the drop in
    // contrast against the board, and for black that means coming up rather than going down.
    const auto *material = capturable[square]          ? "piece_capture"
      : hidden[square] >= 0                            ? (white ? "piece_white_ghost" : "piece_black_ghost")
      : piece.color != board.ToMove()                  ? (white ? "piece_white_dim" : "piece_black_dim")
                                                       : (white ? "piece_white" : "piece_black");
    // Every value below is written whether the entity is new or reused. A square a knight has left
    // and a queen has arrived on keeps one entity, and anything not overwritten here would still be
    // the knight's -- so the rotation is set on all of them rather than only where it is wanted.
    pieces[square] = (pieces[square] ? builder.Entity(pieces[square]) : builder.Entity("piece" + std::to_string(square)))
                       .Mesh(shape.mesh)
                       .Material(material)
                       .Scale(shape.scale)
                       // Standing on its square rather than at a fixed height, so a piece on a
                       // raised square rises with it instead of sinking into the slab.
                       .At({center.x, SquareTop(square) + shape.scale.y * .5f + lift, center.z})
                       // A knight is turned off-axis so its cube is not mistaken for a rook's.
                       .RotateEuler({.0f, piece.type == PieceType::Knight ? 25.f : .0f, .0f})
                       .Id();
  }
}
auto GameManager::Select(const int square) -> void {
  selected = square;
  rise = {};
  capturable = {};
  if (square >= 0) {
    const auto file = Board::FileOf(square);
    const auto rank = Board::RankOf(square);
    for (const auto &move : board.LegalMoves(square)) {
      // Straight-line distance across the board rather than a count of steps, so that a knight's
      // two-and-one lands between a neighbour and a move of that many squares along a rank, which
      // is what it looks like on the board.
      const auto reach = std::hypot(static_cast<float>(Board::FileOf(move.to) - file), static_cast<float>(Board::RankOf(move.to) - rank));
      const auto share = std::clamp((reach - 1.f) / (SQUARE_RISE_RANGE - 1.f), .0f, 1.f);
      rise[move.to] = SQUARE_RISE_MIN + (SQUARE_RISE_MAX - SQUARE_RISE_MIN) * share;
      // The square the taken piece is standing on, which is not the destination for en passant.
      // Marking `to` instead would leave the pawn that is actually in danger looking safe and
      // colour an empty square that no piece is standing on.
      if (move.captured >= 0)
        capturable[move.captured] = true;
    }
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
        if (rise[target] <= .0f)
          continue;
        wanted[target] = true;
        aim[target] = {centre.x, rise[target], centre.z};
      } else {
        const auto piece = board.Get(target);
        if (!piece.Occupied() || piece.color != board.ToMove())
          continue;
        wanted[target] = true;
        // The middle of the piece rather than the square under it, because the piece is what a
        // click picking it up would be aimed at.
        aim[target] = {centre.x, SquareTop(target) + ShapeOf(piece.type).scale.y * .5f, centre.z};
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
      const auto bottom = SquareTop(square);
      const auto radius = std::max(shape.scale.x, shape.scale.z) * .5f;
      auto nearest = std::numeric_limits<float>::max();
      for (auto target = 0; target < Board::SquareCount; ++target) {
        if (!wanted[target] || !Blocks(camera->position, aim[target], standing, radius, bottom, bottom + shape.scale.y))
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
        board.Apply(*move);
        Select(-1);
        UpdateTitle();
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
  // and the raised squares are most of what there is to look at while turning it.
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
  Select(-1);
  UpdateTitle();
  spdlog::info("[Chess] new game");
}
auto GameManager::UpdateTitle() -> void {
  const auto state = board.State();
  playable = state != GameState::Checkmate && state != GameState::Stalemate;
  if (auto *app = GetApp(); app)
    app->SetWindowTitle("Chess -- " + StateText(board, state));
}
KUKI_REGISTER_SCRIPT(GameManager)
