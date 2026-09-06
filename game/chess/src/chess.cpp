#include <camera.hpp>
#include <chess.hpp>
#include <game_builder.hpp>
#include <game_manager.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/gtc/quaternion.hpp>
#include <indirect_lighting.hpp>
#include <launch_path.hpp>
#include <light.hpp>
#include <light_type.hpp>
#include <material_asset.hpp>
#include <spdlog/spdlog.h>
#include <string>
using namespace kuki;
namespace {
/// @brief Materials the game draws with, staged next to the binary as `.mat` files.
///
/// Files rather than values built in code, because `AssetManager` owns asset identity and the
/// only public route into it is a load from a path. A handful of small JSON files is a fair price
/// for not reaching around the manager.
constexpr const char *MATERIALS[]{"piece_white", "piece_black", "piece_check", "piece_checkmate", "piece_draw", "piece_white_ghost", "piece_black_ghost", "square_light", "square_dark", "square_light_move", "square_dark_move", "square_light_capture", "square_dark_capture"};
/// @brief Rotation that points a camera's forward at a target. Forward is -Z of the rotation.
auto LookAt(const glm::vec3 &from, const glm::vec3 &to) -> glm::quat {
  return glm::quatLookAt(glm::normalize(to - from), glm::vec3(.0f, 1.f, .0f));
}
} // namespace
Chess::Chess()
  : SystemApplication(ApplicationDescription{.name = "Kuki Chess"}) {}
auto Chess::Start() -> void {
  for (const auto *name : MATERIALS)
    LoadAsset<MaterialAsset>(ResolvePath(std::string("material/") + name + ".mat"), name);
  // Before the scene, because `GameManager::Start` draws the board's labels out of this same atlas
  // and scripts start after this function returns.
  if (!SetOverlayFont(ResolvePath(CHESS_FONT)))
    spdlog::warn("[Chess] No font, so the clocks and the board's labels will not be drawn");
  // The whole scene skeleton in one expression. `Entity` asks for scene scope, so each call
  // discards the entity frame the previous one left open -- which is what lets these run on after
  // one another without a closing call between them. See `Application::Game`.
  Game("Chess")
    .Scene("Main")
    .Entity("Camera")
    .With<Camera>([](Camera &camera) {
      // The camera's own position and rotation, not the entity's transform: nothing in the engine
      // syncs one into the other, and `Camera::Update` reads these. The editor's camera controller
      // is what does that syncing there, and it is editor code.
      camera.position = {.0f, 9.5f, 8.5f};
      camera.rotation = LookAt(camera.position, glm::vec3(.0f, .0f, .0f));
      camera.fov = 40.f;
      camera.farPlane = 60.f;
    })
    .ActiveCamera()
    .Entity("Sun")
    .With<Light>([](Light &light) {
      light.type = LightType::Directional;
      light.forward = glm::normalize(glm::vec3(-.35f, -1.f, -.45f));
      light.diffuse = {1.f, .97f, .92f};
      light.specular = {1.f, 1.f, 1.f};
      light.intensity = 3.f;
    })
    .Entity("Settings")
    .With<IndirectLighting>([](IndirectLighting &lighting) {
      // There is no skybox here, so the flat fallback is all the ambient the scene gets. The
      // default of 0.03 is tuned for a scene that has one, and leaves this board's shadows black.
      lighting.ambientFallback = .18f;
      // Lighter than the engine's default, because the clocks are drawn in the colours of the
      // pieces and the dark one has to be dark against something. A mid ground is the only kind
      // that can hold both: darker and Black's clock disappears into it, lighter and White's does.
      lighting.backgroundColor = {.34f, .35f, .4f};
    })
    .Entity("GameManager")
    .Script<GameManager>();
}
