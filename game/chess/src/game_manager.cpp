#include <GLFW/glfw3.h>
#include <application.hpp>
#include <cstdlib>
#include <entity_manager.hpp>
#include <game_builder.hpp>
#include <game_manager.hpp>
#include <glm/ext/vector_float3.hpp>
#include <script_registry.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include <transform.hpp>
using namespace kuki;
namespace {
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
auto StateText(const Board &board) -> std::string {
  const auto mover = board.ToMove() == PieceColor::White ? "White" : "Black";
  const auto winner = board.ToMove() == PieceColor::White ? "Black" : "White";
  switch (board.State()) {
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
  : Script(std::in_place_type<GameManager>) {}
auto GameManager::CloneTo(EntityManager &entityManager, const EntityID id) const -> void {
  auto *script = entityManager.AddComponent<GameManager>(id);
  // The board is worth copying; the entities it was drawn with belong to the original and the
  // input actions belong to whoever registered them, so both are left for `Start` to redo.
  script->board = board;
  script->pieces = {};
  script->markers.clear();
  script->selected = -1;
  script->clickAction = InputManager::InvalidActionID;
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
  BuildBoard();
  RebuildPieces();
  clickAction = app->RegisterInputAction(GLFW_MOUSE_BUTTON_LEFT, [this]() { OnClick(); });
  quitAction = app->RegisterInputAction(GLFW_KEY_ESCAPE, [this]() {
    // Through the base rather than captured: an input action outlives the call that registered it.
    if (auto *self = GetApp(); self)
      self->Quit();
  });
  restartAction = app->RegisterInputAction(GLFW_KEY_R, [this]() { Restart(); });
  UpdateTitle();
  spdlog::info("[Chess] click a piece to select it, click a marker to move; R restarts, Escape quits");
}
auto GameManager::Update(Application &) -> void {}
auto GameManager::Shutdown(Application &application) -> void {
  // Unregistered explicitly, because the actions capture `this` and the input manager outlives the
  // scene: a stale action would call into a destroyed script on the next click.
  for (const auto action : {clickAction, quitAction, restartAction})
    if (action != InputManager::InvalidActionID)
      application.UnregisterInputAction(action);
  clickAction = InputManager::InvalidActionID;
  quitAction = InputManager::InvalidActionID;
  restartAction = InputManager::InvalidActionID;
}
auto GameManager::BuildBoard() -> void {
  auto *app = GetApp();
  if (!app)
    return;
  // Squares are named by their index so that `SquareOfEntity` can find one by name rather than by
  // keeping a second map in step with this loop.
  auto builder = app->Game().Scene("Main");
  for (auto square = 0; square < Board::SquareCount; ++square) {
    const auto light = (Board::FileOf(square) + Board::RankOf(square)) % 2 == 1;
    const auto center = SquareCenter(square);
    builder.Entity("square" + std::to_string(square))
      .Mesh("Cube")
      .Material(light ? "square_light" : "square_dark")
      .Scale({.98f * SQUARE_SIZE, .2f, .98f * SQUARE_SIZE})
      .At({center.x, -.1f, center.z});
  }
}
auto GameManager::RebuildPieces() -> void {
  auto *app = GetApp();
  if (!app)
    return;
  for (auto &piece : pieces) {
    if (piece)
      app->DeleteEntity(piece);
    piece = {};
  }
  auto builder = app->Game().Scene("Main");
  for (auto square = 0; square < Board::SquareCount; ++square) {
    const auto piece = board.Get(square);
    if (!piece.Occupied())
      continue;
    const auto shape = ShapeOf(piece.type);
    const auto center = SquareCenter(square);
    const auto lift = selected == square ? .25f : .0f;
    auto entity = builder.Entity("piece" + std::to_string(square))
                    .Mesh(shape.mesh)
                    .Material(piece.color == PieceColor::White ? "piece_white" : "piece_black")
                    .Scale(shape.scale)
                    .At({center.x, shape.scale.y * .5f + lift, center.z});
    if (piece.type == PieceType::Knight)
      // Turned off-axis so a knight's cube is not mistaken for a rook's at a glance.
      entity = entity.RotateEuler({.0f, 25.f, .0f});
    pieces[square] = entity.Id();
  }
}
auto GameManager::ClearMarkers() -> void {
  if (auto *app = GetApp(); app)
    for (const auto marker : markers)
      if (marker)
        app->DeleteEntity(marker);
  markers.clear();
}
auto GameManager::Select(const int square) -> void {
  auto *app = GetApp();
  if (!app)
    return;
  ClearMarkers();
  selected = square;
  if (square >= 0) {
    auto builder = app->Game().Scene("Main");
    for (const auto &move : board.LegalMoves(square)) {
      const auto center = SquareCenter(move.to);
      const auto capture = move.captured >= 0;
      markers.push_back(builder.Entity("marker" + std::to_string(move.to))
          .Mesh("Cube")
          .Material(capture ? "marker_capture" : "marker_move")
          .Scale({.3f, .06f, .3f})
          .At({center.x, .06f, center.z})
          .Id());
    }
  }
  // The lift on the selected piece is part of how a piece is placed, so the pieces are rebuilt
  // rather than nudged. See `RebuildPieces`.
  RebuildPieces();
}
auto GameManager::SquareOfEntity(const EntityID id) const -> int {
  if (!id)
    return -1;
  for (auto square = 0; square < Board::SquareCount; ++square)
    if (pieces[square] == id)
      return square;
  auto *app = GetApp();
  if (!app)
    return -1;
  // Not a piece, so it is either a square or a marker, both of which carry their index in the
  // name. Matching on the name rather than on a third map: the squares never move or die, so a map
  // would only be a copy of what the entity already knows about itself.
  const auto name = app->GetEntityName(id);
  for (const auto *prefix : {"square", "marker"}) {
    const std::string tag(prefix);
    if (name.starts_with(tag)) {
      const auto index = std::atoi(name.c_str() + tag.size());
      if (index >= 0 && index < Board::SquareCount)
        return index;
    }
  }
  return -1;
}
auto GameManager::OnClick() -> void {
  auto *app = GetApp();
  if (!app)
    return;
  const auto state = board.State();
  if (state == GameState::Checkmate || state == GameState::Stalemate)
    return;
  const auto square = SquareOfEntity(app->PickEntity(app->GetMousePosition()));
  if (square < 0) {
    Select(-1);
    return;
  }
  if (selected >= 0)
    if (const auto move = board.Legal(selected, square); move) {
      board.Apply(*move);
      selected = -1;
      ClearMarkers();
      RebuildPieces();
      UpdateTitle();
      return;
    }
  // Not a move, so it is either a new selection or a click on nothing that matters. Selecting only
  // pieces of the side to move is what keeps a player from dragging their opponent's pieces about.
  const auto piece = board.Get(square);
  Select(piece.Occupied() && piece.color == board.ToMove() ? square : -1);
}
auto GameManager::Restart() -> void {
  board.Reset();
  selected = -1;
  ClearMarkers();
  RebuildPieces();
  UpdateTitle();
  spdlog::info("[Chess] new game");
}
auto GameManager::UpdateTitle() const -> void {
  if (auto *app = GetApp(); app)
    app->SetWindowTitle("Chess -- " + StateText(board));
}
KUKI_REGISTER_SCRIPT(GameManager)
