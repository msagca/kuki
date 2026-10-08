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
#include <glm/common.hpp>
#include <glm/exponential.hpp>
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
/// The meshes are the Blender models staged under `model/`, built to the board's units and standing
/// on their origins, so the radius and height here are the models' own -- read off the exports,
/// and needing to change with them. They bound the piece for `UpdateOcclusion` and nothing else:
/// the drawn size is the model's, at unit scale. Rank still reads as height, king tallest.
struct PieceShape {
  const char *mesh;
  float radius;
  float height;
};
auto ShapeOf(const PieceType type) -> PieceShape {
  switch (type) {
  case PieceType::Pawn:
    return {"pawn", .27f, .6f};
  case PieceType::Knight:
    return {"knight", .3f, .87f};
  case PieceType::Bishop:
    return {"bishop", .29f, .95f};
  case PieceType::Rook:
    return {"rook", .3f, .76f};
  case PieceType::Queen:
    return {"queen", .32f, 1.14f};
  case PieceType::King:
  default:
    return {"king", .33f, 1.36f};
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

/// @brief Colours the code draws with: the board's own blue, for the chosen time control, and the
/// red its squares take when something can be captured there, for a clock that is running out.
///
/// The `.mat` files carry the board's numbers as albedo. They are data and cannot include a header,
/// so a colour changed here has to be changed there too.
///
/// Worth knowing when reading the values: a material's albedo is lit and tone mapped before anyone
/// sees it, while overlay text is written straight onto the finished picture. The same number
/// therefore arrives lighter on the board than it does in a caption, which is why the text is
/// carried at less than full opacity rather than at a lightened shade of its own.
constexpr glm::vec3 HIGHLIGHT_RED{.66f, .16f, .14f};
/// @brief The green a square showing a legal move is mixed toward, and how far.
///
/// Mixed rather than flat, because a flat green over both shades would say where the piece may go
/// and take the board's pattern away while it said it, which is most of what tells one square from
/// the next. These are the numbers the blue theme's `.mat` files were drawn with before themes
/// existed, worked back out of them -- so the blue board looks exactly as it did.
constexpr glm::vec3 MOVE_GREEN{.24f, .58f, .28f};
constexpr auto HIGHLIGHT_MIX = .7f;
/// @brief A look for the board: its two shades of square and its frame.
///
/// The chosen time option is drawn in the theme's dark shade, so the settings and the board agree
/// by construction rather than by two people picking similar numbers.
struct BoardTheme {
  const char *key;
  const char *name;
  glm::vec3 light;
  glm::vec3 dark;
  glm::vec3 frame;
  float roughness;
};
/// @brief The themes on offer, first being the default.
///
/// Blue is the board the game shipped with. Walnut is a club board: maple and walnut, with a frame
/// in the darker wood and a satin finish -- flat colour, since a `.mat` cannot carry the grain.
/// Slate is a stone board, cool and polished, and Violet is there for a look none of the others
/// gives. Green, the commonest tournament colour, is missing on purpose: a move is shown by mixing
/// toward green, and on a green board that says nothing.
constexpr BoardTheme THEMES[]{
  {"blue", "Blue", {.78f, .74f, .62f}, {.2f, .3f, .52f}, {.23f, .14f, .08f}, .8f},
  {"walnut", "Walnut", {.8f, .62f, .4f}, {.42f, .24f, .12f}, {.34f, .19f, .09f}, .55f},
  {"slate", "Slate", {.8f, .8f, .78f}, {.3f, .31f, .34f}, {.12f, .12f, .13f}, .35f},
  {"violet", "Violet", {.8f, .76f, .84f}, {.42f, .3f, .55f}, {.18f, .13f, .22f}, .7f},
};
/// @brief Name of a theme's material for one shade of square, plain or highlighted.
///
/// `kind` is empty for the plain square, or `_move` or `_capture`.
auto SquareMaterial(const BoardTheme &theme, const bool light, const std::string_view kind) -> std::string {
  return std::string("square_") + theme.key + (light ? "_light" : "_dark") + std::string(kind);
}
auto FrameMaterial(const BoardTheme &theme) -> std::string {
  return std::string("board_frame_") + theme.key;
}
/// @brief Names of a theme's clock materials: the body in the frame's colour and the screens and the
/// rocker in the light shade, so the clock reads as a piece of the same set. The light shade
/// rather than the dark for the rocker because in some themes the dark square and the frame are
/// near the same colour, and the rocker is the part that has to be seen to move.
auto ClockMaterial(const BoardTheme &theme, const std::string_view part) -> std::string {
  return std::string("clock_") + std::string(part) + "_" + theme.key;
}
/// @brief A material's albedo as an overlay colour.
///
/// The overlay writes its colours straight onto the finished picture, while the board's albedo is
/// linear and arrives lit and tone mapped. Taken to the display's curve, a swatch reads as the
/// shade the board is drawn in rather than as a darker cousin of it.
auto Swatch(const glm::vec3 &albedo) -> glm::vec4 {
  return {glm::pow(albedo, glm::vec3(1.f / 2.2f)), 1.f};
}
/// @brief What the clock's figures are printed in, running and waiting.
///
/// Printed rather than lit: albedo and not emission, like ink on a reflective display. The screens
/// they sit on are the theme's light shade, so these are neutral and read on any of them. Near black
/// for the time that is counting, a mid grey for the one that is waiting -- both times are worth
/// knowing at a glance, and it is the pair being unequal that says whose move it is. The board's
/// capture red, darkened to ink, once a time is low.
constexpr glm::vec3 CLOCK_INK{.02f, .02f, .02f};
constexpr glm::vec3 CLOCK_INK_IDLE{.34f, .34f, .34f};
constexpr glm::vec3 CLOCK_INK_LOW{HIGHLIGHT_RED * .7f};
constexpr glm::vec3 CLOCK_INK_LOW_IDLE{.4f, .24f, .2f};
/// @brief The option in force, the one under the pointer, and the ones merely on offer.
///
/// Hover is the chosen blue at less than full strength rather than a colour of its own, so that
/// pointing at an option previews what choosing it would look like instead of introducing a third
/// thing to learn.
constexpr glm::vec4 OPTION_IDLE{1.f, 1.f, 1.f, .35f};
/// @brief Index into anything kept one per side, White first.
auto SideIndex(const PieceColor color) -> size_t {
  return color == PieceColor::White ? 0 : 1;
}
/// @brief A clock as the characters on its screen, `MM:SS`, rounded up so that it reads 0:00 only
/// once it is spent. The tens of minutes are a space below ten, as a clock's display leaves them.
///
/// Capped at 99:59, which is all five slots can say. The longest control on offer is half an hour,
/// so only an increment collected over a very long game could reach it.
auto ClockGlyphs(const float seconds) -> std::array<char, 5> {
  const auto total = std::min(static_cast<int>(std::ceil(std::max(seconds, .0f))), 99 * 60 + 59);
  const auto minutes = total / 60;
  const auto rest = total % 60;
  return {minutes >= 10 ? static_cast<char>('0' + minutes / 10) : ' ', static_cast<char>('0' + minutes % 10), ':', static_cast<char>('0' + rest / 10), static_cast<char>('0' + rest % 10)};
}
/// @brief Name of the glyph mesh a clock slot shows for a character.
auto GlyphName(const char c) -> std::string {
  return std::string("clock_glyph_") + c;
}
/// @brief Registers a material that draws text cut from the overlay's atlas.
///
/// A cutout rather than a blend. The atlas is white everywhere with the glyph in its alpha, so
/// what is wanted is the ink and nothing else -- and a cutout writes depth and needs no sorting,
/// where a blended quad would have to be drawn after everything opaque.
auto AddTextMaterial(Application &app, const AssetID atlasId, const std::string &name, const glm::vec4 &albedo, const glm::vec3 &emissive) -> AssetID {
  const auto materialId = MakeBuiltInAssetID("chess/" + name);
  auto materialAsset = std::make_unique<MaterialAsset>(materialId);
  materialAsset->type = MaterialType::Lit;
  materialAsset->fallback.albedo = albedo;
  materialAsset->fallback.emissive = {emissive, 1.f};
  materialAsset->fallback.metalness = .0f;
  materialAsset->fallback.roughness = .9f;
  materialAsset->fallback.alphaMode = AlphaMode::Mask;
  materialAsset->fallback.alphaCutoff = .5f;
  materialAsset->textures.push_back(atlasId);
  app.AddAsset(std::move(materialAsset), name);
  return materialId;
}
auto FlagText(const size_t side) -> std::string {
  return side == 0 ? "White is out of time, Black wins" : "Black is out of time, White wins";
}
/// @brief The letter a piece is written with. Pawns are written by their file, so they have none.
auto PieceLetter(const PieceType type) -> const char * {
  switch (type) {
  case PieceType::Knight:
    return "N";
  case PieceType::Bishop:
    return "B";
  case PieceType::Rook:
    return "R";
  case PieceType::Queen:
    return "Q";
  case PieceType::King:
    return "K";
  default:
    return "";
  }
}
auto SquareName(const int square) -> std::string {
  return std::string(1, static_cast<char>('a' + Board::FileOf(square))) + static_cast<char>('1' + Board::RankOf(square));
}
/// @brief A move written the way a game score writes it, read from the position before it is made.
///
/// Before, necessarily: what a move is called depends on what else was legal at the time. A knight
/// written `Nf3` when the other knight could also have reached f3 names neither of them, so the
/// file or the rank it came from has to be added -- and once the move has been applied, the other
/// knight's options are the answer to a different question.
///
/// Check and mate are the other way round and are appended by the caller, since neither is knowable
/// until the move has been made.
auto Notation(const Board &board, const Move &move) -> std::string {
  // The rook tells the sides apart: it starts outside the king on both, so the one that ends up
  // nearer the centre of the board is the short castle.
  if (move.Castle())
    return Board::FileOf(move.rookFrom) > Board::FileOf(move.from) ? "O-O" : "O-O-O";
  const auto piece = board.Get(move.from);
  const auto pawn = piece.type == PieceType::Pawn;
  std::string text = PieceLetter(piece.type);
  if (pawn) {
    // A pawn is named by the file it came from, and only when it takes something.
    if (move.captured >= 0)
      text += static_cast<char>('a' + Board::FileOf(move.from));
  } else {
    auto ambiguous = false;
    auto sharesFile = false;
    auto sharesRank = false;
    for (auto square = 0; square < Board::SquareCount; ++square) {
      if (square == move.from)
        continue;
      const auto other = board.Get(square);
      if (other.type != piece.type || other.color != piece.color || !board.Legal(square, move.to))
        continue;
      ambiguous = true;
      sharesFile = sharesFile || Board::FileOf(square) == Board::FileOf(move.from);
      sharesRank = sharesRank || Board::RankOf(square) == Board::RankOf(move.from);
    }
    // The file where that tells them apart, the rank where it does not, and both where neither
    // does -- which takes three pieces of a kind and is why the last case is not dead code.
    if (ambiguous) {
      if (!sharesFile)
        text += static_cast<char>('a' + Board::FileOf(move.from));
      else if (!sharesRank)
        text += static_cast<char>('1' + Board::RankOf(move.from));
      else
        text += SquareName(move.from);
    }
  }
  if (move.captured >= 0)
    text += 'x';
  text += SquareName(move.to);
  // Always a queen; see `Board`, which does not offer the choice.
  if (move.promotion)
    text += "=Q";
  return text;
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
  script->frame = {};
  script->clock = {};
  script->faces = {};
  script->clockBody = {};
  script->clockScreen = {};
  script->rocker = {};
  script->rockerAngle = .0f;
  script->clocks = {StartSeconds(), StartSeconds()};
  script->timeOption = timeOption;
  script->incrementOption = incrementOption;
  script->flagged = -1;
  script->moves = {};
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
  CreateThemeMaterials();
  CreateFrame();
  CreateLabels();
  CreateClock();
  // Before `Refresh`, which draws the pieces and reads the king's colour off `state`.
  UpdateStanding();
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
  UpdateClock(application);
  DrawOptions(application);
  DrawThemes(application);
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
  const auto theme = preferences.GetString(PREF_THEME, THEMES[themeOption].key);
  for (size_t i = 0; i < std::size(THEMES); ++i)
    if (theme == THEMES[i].key)
      themeOption = i;
}
auto GameManager::SaveSettings(Application &application) -> void {
  auto &preferences = application.GetPreferences();
  preferences.Set(PREF_MINUTES, TIME_OPTIONS[timeOption]);
  preferences.Set(PREF_INCREMENT, INCREMENT_OPTIONS[incrementOption]);
  preferences.Set(PREF_THEME, std::string(THEMES[themeOption].key));
}
auto GameManager::PushMove(std::string text) -> void {
  // Oldest first, so the list reads downward the way a game score does. Shifting three strings a
  // move is not worth a ring buffer and the index arithmetic that comes with one.
  for (size_t i = 1; i < moves.size(); ++i)
    moves[i - 1] = std::move(moves[i]);
  moves.back() = std::move(text);
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
  const glm::vec4 active{THEMES[themeOption].dark, 1.f};
  const glm::vec4 hover{THEMES[themeOption].dark, .7f};
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
      const auto tint = i == chosen ? active : id == hovered ? hover
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
  if (Choose(TIME_OPTION_ID, std::size(TIME_OPTIONS), timeOption) || Choose(INCREMENT_OPTION_ID, std::size(INCREMENT_OPTIONS), incrementOption))
    return true;
  // A theme is not a reason to throw the game away either, so this one is applied in place.
  const auto theme = static_cast<size_t>(id - THEME_OPTION_ID);
  if (id < THEME_OPTION_ID || theme >= std::size(THEMES))
    return false;
  if (theme != themeOption) {
    themeOption = theme;
    if (auto *app = GetApp(); app)
      SaveSettings(*app);
    ApplyTheme();
  }
  return true;
}
auto GameManager::UpdateClocks(Application &application) -> void {
  if (!playable)
    return;
  const auto side = SideIndex(board.ToMove());
  clocks[side] = std::max(clocks[side] - application.DeltaTime(), .0f);
  if (clocks[side] > .0f)
    return;
  flagged = static_cast<int>(side);
  UpdateStanding();
  // Whatever was in hand is put down: the game is over, and a piece left lifted would go on
  // offering moves that can no longer be played.
  Select(-1);
  spdlog::info("[Chess] {}", FlagText(side));
}
auto GameManager::UpdateClock(Application &application) -> void {
  auto *app = GetApp();
  if (!app || !clock)
    return;
  // Asked for only on a frame that changes something. This runs every frame and almost every frame
  // changes nothing -- a reading moves once a second -- and naming the scene is a scene switch.
  const auto Builder = [app] { return app->Game().Scene("Main"); };
  for (size_t side = 0; side < faces.size(); ++side) {
    const auto color = side == 0 ? PieceColor::White : PieceColor::Black;
    // White's half is the model's +x; see `CLOCK_YAW`.
    const auto x = side == 0 ? SCREEN_X : -SCREEN_X;
    // Black while it is counting, and black once more when it is the clock that ran out -- a fallen
    // flag is the one reading worth more attention than the side to move.
    const auto counting = playable ? color == board.ToMove() : flagged == static_cast<int>(side);
    const auto low = clocks[side] <= CLOCK_LOW;
    const std::string_view material = low ? (counting ? "clock_ink_low" : "clock_ink_low_idle") : (counting ? "clock_ink" : "clock_ink_idle");
    const auto restyle = faces[side].material != material;
    faces[side].material = material;
    const auto glyphs = ClockGlyphs(clocks[side]);
    for (size_t slot = 0; slot < CLOCK_SLOTS; ++slot) {
      auto &id = faces[side].slots[slot];
      const auto c = glyphs[slot];
      if (c == ' ') {
        if (id)
          app->DeleteEntity(id);
        id = {};
        faces[side].shown[slot] = c;
        continue;
      }
      if (id && c == faces[side].shown[slot] && !restyle)
        continue;
      faces[side].shown[slot] = c;
      if (id) {
        Builder().Entity(id).Mesh(GlyphName(c)).Material(std::string(material));
        continue;
      }
      // On the glass, leaning with it. The glyph meshes face +z and the tilt about x lays them on
      // the screen's slope, so each slot is only ever placed along the model's x.
      id = Builder()
             .Entity(clock)
             .Child("clock" + std::to_string(side) + "_" + std::to_string(slot))
             .Mesh(GlyphName(c))
             .Material(std::string(material))
             .Scale(glyphSize)
             .At({x + slotOffsets[slot], SCREEN_Y, SCREEN_Z})
             .RotateEuler({SCREEN_TILT, .0f, .0f})
             .Id();
    }
  }
  // Down at the end of whoever is waiting: they pressed it to hand the move over, which is what
  // making a move does here. Once the game is over it stays as the last move left it.
  const auto target = board.ToMove() == PieceColor::White ? ROCKER_TILT : -ROCKER_TILT;
  if (rockerAngle == target || !rocker)
    return;
  rockerAngle += (target - rockerAngle) * (1.f - std::exp(-ROCKER_SPEED * application.DeltaTime()));
  if (std::abs(target - rockerAngle) < 1e-3f)
    rockerAngle = target;
  Builder().Entity(rocker).RotateEuler({.0f, .0f, rockerAngle});
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
    const auto material = SquareMaterial(THEMES[themeOption], light, capturable[square] ? "_capture" : reachable[square] ? "_move"
                                                                                                                      : "");
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
  // Off-white and matt, so the labels read as painted onto the frame rather than as another piece.
  const auto materialId = AddTextMaterial(*app, atlasId, "label", {.86f, .84f, .79f, 1.f}, glm::vec3(.0f));
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
             // Just above the frame's top face, and a quarter turn about x to lay the
             // glyphs face-up: that turn takes the mesh's +z normal to +y and its +y to -z, so the
             // text both faces the sky and reads from White's side of the board.
             .At({.0f, LABEL_LIFT, .0f})
             .RotateEuler({-90.f, .0f, .0f})
             .Id();
}
auto GameManager::CreateFrame() -> void {
  auto *app = GetApp();
  if (!app || frame)
    return;
  // The frame's top is level with the squares' and its recess is exactly their size and depth, so
  // it is placed at the origin and the squares drop into it as they are.
  frame = app->Game().Scene("Main").Entity("Frame").Mesh("board").Material(FrameMaterial(THEMES[themeOption])).At({.0f, .0f, .0f}).Id();
}
auto GameManager::CreateThemeMaterials() -> void {
  auto *app = GetApp();
  if (!app)
    return;
  const auto Add = [&](const std::string &name, const glm::vec3 &albedo, const glm::vec3 &emissive, const float roughness) {
    // Registered once per run. A second `Start` -- a cloned scene -- finds them already there.
    if (app->GetAsset(name))
      return;
    auto material = std::make_unique<MaterialAsset>(MakeBuiltInAssetID("chess/" + name));
    material->type = MaterialType::Lit;
    material->fallback.albedo = {albedo, 1.f};
    material->fallback.emissive = {emissive, 1.f};
    material->fallback.metalness = .0f;
    material->fallback.roughness = roughness;
    app->AddAsset(std::move(material), name);
  };
  for (const auto &theme : THEMES) {
    for (const auto light : {true, false}) {
      const auto base = light ? theme.light : theme.dark;
      // The faint glow keeps a highlighted square reading as lit on the shaded side of the board.
      // The light shade carries a little more, as it did in the files these replace.
      Add(SquareMaterial(theme, light, ""), base, glm::vec3(.0f), theme.roughness);
      Add(SquareMaterial(theme, light, "_move"), glm::mix(base, MOVE_GREEN, HIGHLIGHT_MIX), light ? glm::vec3(.03f, .1f, .04f) : glm::vec3(.02f, .07f, .03f), .75f);
      Add(SquareMaterial(theme, light, "_capture"), glm::mix(base, HIGHLIGHT_RED, HIGHLIGHT_MIX), light ? glm::vec3(.12f, .01f, .01f) : glm::vec3(.09f, .01f, .01f), .75f);
    }
    Add(FrameMaterial(theme), theme.frame, glm::vec3(.0f), theme.roughness);
    // Smoother than the board, as a moulded case is; the screens most of all, like glass.
    Add(ClockMaterial(theme, "case"), theme.frame, glm::vec3(.0f), .4f);
    Add(ClockMaterial(theme, "screen"), theme.light, glm::vec3(.0f), .25f);
    Add(ClockMaterial(theme, "lever"), theme.light, glm::vec3(.0f), .35f);
  }
}
auto GameManager::ApplyTheme() -> void {
  auto *app = GetApp();
  if (!app)
    return;
  const auto &theme = THEMES[themeOption];
  if (frame)
    app->Game().Scene("Main").Entity(frame).Material(FrameMaterial(theme));
  if (clockBody)
    app->Game().Scene("Main").Entity(clockBody).Material(ClockMaterial(theme, "case"));
  if (clockScreen)
    app->Game().Scene("Main").Entity(clockScreen).Material(ClockMaterial(theme, "screen"));
  if (rocker)
    app->Game().Scene("Main").Entity(rocker).Material(ClockMaterial(theme, "lever"));
  RefreshSquares();
}
auto GameManager::DrawThemes(Application &application) -> void {
  auto &overlay = application.GetOverlay();
  const auto &font = overlay.GetFont();
  if (!font.IsLoaded())
    return;
  const auto hovered = application.PickOverlay(application.GetMousePosition());
  for (size_t i = 0; i < std::size(THEMES); ++i) {
    const auto &theme = THEMES[i];
    const auto id = THEME_OPTION_ID + static_cast<int>(i);
    const auto chosen = i == themeOption;
    // A row to a theme, level with the time options' rows, measured in from the right edge.
    const auto y = OPTION_MARGIN + static_cast<float>(i) * OPTION_ROW_STEP;
    const auto right = -OPTION_MARGIN;
    // The chosen pair is ringed and the one under the pointer faintly so: a ring rather than a
    // change to the swatches, because the swatches are the thing being compared and must not
    // change colour to say which one is chosen.
    if (chosen || id == hovered)
      overlay.DrawRect({right + SWATCH_RING, y - SWATCH_RING}, {SWATCH_SIZE * 2.f + SWATCH_RING * 2.f, SWATCH_SIZE + SWATCH_RING * 2.f}, {1.f, 1.f, 1.f, chosen ? .9f : .4f}, TextAnchor::TopRight, id);
    // Light then dark, the way the board's own corner reads from White's side.
    overlay.DrawRect({right - SWATCH_SIZE, y}, glm::vec2(SWATCH_SIZE), Swatch(theme.light), TextAnchor::TopRight, id);
    overlay.DrawRect({right, y}, glm::vec2(SWATCH_SIZE), Swatch(theme.dark), TextAnchor::TopRight, id);
    // The name to the left, its ink centred on the swatches. None of the names has a descender,
    // so the ink's height is the cap height and the centring holds row to row.
    glm::vec2 min, max;
    if (!font.Bounds(theme.name, min, max))
      continue;
    const auto ink = (max.y - min.y) * THEME_TEXT_SIZE;
    const glm::vec4 tint = chosen ? glm::vec4(1.f, 1.f, 1.f, .95f) : id == hovered ? glm::vec4(1.f, 1.f, 1.f, .7f)
                                                                                    : OPTION_IDLE;
    overlay.DrawText(theme.name, {right - SWATCH_SIZE * 2.f - SWATCH_RING - OPTION_SPACING, y + (SWATCH_SIZE - ink) * .5f}, THEME_TEXT_SIZE, tint, TextAnchor::TopRight, id);
  }
}
auto GameManager::CreateClock() -> void {
  auto *app = GetApp();
  if (!app || clock)
    return;
  const auto &font = app->GetOverlay().GetFont();
  const auto atlasId = app->GetOverlay().GetAtlasAssetId();
  // Without the font the screen would be blank, but the clock still stands and its rocker still
  // says whose move it is.
  if (font.IsLoaded() && atlasId) {
    const auto inkId = AddTextMaterial(*app, atlasId, "clock_ink", {CLOCK_INK, 1.f}, glm::vec3(.0f));
    AddTextMaterial(*app, atlasId, "clock_ink_idle", {CLOCK_INK_IDLE, 1.f}, glm::vec3(.0f));
    AddTextMaterial(*app, atlasId, "clock_ink_low", {CLOCK_INK_LOW, 1.f}, glm::vec3(.0f));
    AddTextMaterial(*app, atlasId, "clock_ink_low_idle", {CLOCK_INK_LOW_IDLE, 1.f}, glm::vec3(.0f));
    // Every digit takes the widest digit's cell, as a clock's display does, so a reading never
    // shifts sideways as it counts down. The colon gets its own advance, which is narrower.
    auto digitAdvance = .0f;
    for (auto c = '0'; c <= '9'; ++c)
      digitAdvance = std::max(digitAdvance, font.Measure(std::string(1, c)));
    const auto colonAdvance = font.Measure(":");
    const std::array<float, CLOCK_SLOTS> cells{digitAdvance, digitAdvance, colonAdvance, digitAdvance, digitAdvance};
    auto total = .0f;
    for (const auto cell : cells)
      total += cell;
    if (total > .0f) {
      glyphSize = SCREEN_TEXT_WIDTH / total;
      auto left = -total * .5f;
      for (size_t slot = 0; slot < CLOCK_SLOTS; ++slot) {
        slotOffsets[slot] = (left + cells[slot] * .5f) * glyphSize;
        left += cells[slot];
      }
    }
    // Every digit is centred on the ink of a zero rather than its own, so the figures share a
    // baseline and a cap height the way printed figures do; a 7 centred on itself would sit high.
    // The colon is centred on its own ink, which is what puts it between the figures' middles.
    glm::vec2 zeroMin, zeroMax;
    const auto digitMiddle = font.Bounds("0", zeroMin, zeroMax) ? (zeroMin.y + zeroMax.y) * .5f : .0f;
    for (const auto c : std::string_view("0123456789:")) {
      const std::string text(1, c);
      glm::vec2 min, max;
      if (!font.Bounds(text, min, max))
        continue;
      const auto middle = c == ':' ? (min.y + max.y) * .5f : digitMiddle;
      Mesh mesh;
      font.Append(text, mesh, {-(min.x + max.x) * .5f, -middle});
      auto meshAsset = std::make_unique<MeshAsset>(MakeBuiltInAssetID("chess/" + GlyphName(c)));
      meshAsset->material = inkId;
      meshAsset->bounds = BoundingBox::Calculate(mesh.vertices);
      meshAsset->mesh = std::move(mesh);
      app->AddAsset(std::move(meshAsset), GlyphName(c));
    }
  }
  clock = app->Game().Scene("Main").Entity("Clock").At(CLOCK_POSITION).RotateEuler({.0f, CLOCK_YAW, .0f}).Id();
  // Children of the clock, so the model's own coordinates place them -- the numbers in the header
  // are the ones Blender printed, and they hold wherever the clock is put.
  const auto &theme = THEMES[themeOption];
  clockBody = app->Game().Scene("Main").Entity(clock).Child("ClockBody").Mesh("clock_body").Material(ClockMaterial(theme, "case")).Id();
  clockScreen = app->Game().Scene("Main").Entity(clock).Child("ClockScreen").Mesh("clock_screen").Material(ClockMaterial(theme, "screen")).Id();
  // Placed by its pivot, which is its origin, so leaning it is a rotation and nothing else.
  rocker = app->Game().Scene("Main").Entity(clock).Child("ClockRocker").Mesh("clock_rocker").Material(ClockMaterial(theme, "lever")).At(ROCKER_PIVOT).RotateEuler({.0f, .0f, rockerAngle}).Id();
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
    // The kings are the only pieces that say anything about themselves, and the only ones with
    // anything left to say: everything about a move is on the squares now.
    //
    // A draw is the one standing both of them report. Check and mate belong to the side facing
    // them -- which is the side to move, since that is whose position `Board::State` describes --
    // but a draw is not something that happened to one player, so colouring one king would be
    // saying it was.
    const auto king = piece.type == PieceType::King;
    const auto facing = king && piece.color == board.ToMove();
    // Ahead of the ghost, because a king with something to report is worth seeing through a piece
    // standing in front of it rather than instead of it.
    const auto *material = king && Drawn(state) ? "piece_draw"
      : facing && state == GameState::Checkmate ? "piece_checkmate"
      : facing && state == GameState::Check     ? "piece_check"
      : hidden[square] >= 0                     ? (white ? "piece_white_ghost" : "piece_black_ghost")
                                                : (white ? "piece_white" : "piece_black");
    // Every value below is written whether the entity is new or reused. A square a knight has left
    // and a queen has arrived on keeps one entity, and anything not overwritten here would still be
    // the knight's -- so the rotation is set on all of them rather than only where it is wanted.
    pieces[square] = (pieces[square] ? builder.Entity(pieces[square]) : builder.Entity("piece" + std::to_string(square)))
                       .Mesh(shape.mesh)
                       .Material(material)
                       .Scale(1.f)
                       // Standing on the board's surface, which is what the slabs' upper faces
                       // are, and what each model's origin is the middle of the base of.
                       .At({center.x, lift, center.z})
                       // The models face -z, which is toward Black, so Black's are turned round to
                       // face White. Only the knight and the bishop's mitre show it, but a knight
                       // looking back over its own shoulder is the one thing that would be noticed.
                       .RotateEuler({.0f, white ? .0f : 180.f, .0f})
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
        aim[target] = {centre.x, ShapeOf(piece.type).height * .5f, centre.z};
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
      auto nearest = std::numeric_limits<float>::max();
      for (auto target = 0; target < Board::SquareCount; ++target) {
        if (!wanted[target] || !Blocks(camera->position, aim[target], standing, shape.radius, .0f, shape.height))
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
        // Written before the move is made, which is when the position can still say what else was
        // legal. See `Notation`.
        auto text = Notation(board, *move);
        board.Apply(*move);
        clocks[mover] += IncrementSeconds();
        // First, because everything below reads what it works out: the check mark on the notation,
        // the king's colour when `Select` redraws, and whether the game is still playable.
        UpdateStanding();
        if (state == GameState::Checkmate)
          text += '#';
        else if (state == GameState::Check)
          text += '+';
        // Then recorded, and only then written up -- the title shows the move that was just made,
        // so it cannot be built before the move reaches the list.
        PushMove(std::move(text));
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
  moves = {};
  // Before `Select` for the reason given there: the pieces are drawn from `state`, and a new game
  // whose standing still said checkmate would open with a red king.
  UpdateStanding();
  UpdateTitle();
  Select(-1);
  spdlog::info("[Chess] New game");
}
auto GameManager::UpdateStanding() -> void {
  state = board.State();
  playable = flagged < 0 && state != GameState::Checkmate && state != GameState::Stalemate;
}
auto GameManager::UpdateTitle() -> void {
  auto *app = GetApp();
  if (!app)
    return;
  // The name, then the moves, and nothing else. What the title used to say is now said better
  // elsewhere: whose move it is by which clock is bright, check by the `+` the notation already
  // writes, and a fallen flag by a clock reading nothing in red.
  std::string title = "Kuki Chess";
  // Most recent first, and as many as the budget allows. The first is taken whatever its length --
  // the move just played is the one worth showing, and a title with nothing after the bar would be
  // a worse answer than one slightly over it.
  std::string played;
  for (auto move = moves.rbegin(); move != moves.rend(); ++move) {
    if (move->empty())
      continue;
    if (!played.empty() && played.size() + 1 + move->size() > TITLE_MOVE_BUDGET)
      break;
    played += played.empty() ? *move : " " + *move;
  }
  if (!played.empty())
    title += " | " + played;
  app->SetWindowTitle(title);
}
KUKI_REGISTER_SCRIPT(GameManager)
