#include <atmosphere_sky.hpp>
#include <bounding_box.hpp>
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
#include <memory>
#include <mesh_asset.hpp>
#include <model_asset.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include <volumetric_clouds.hpp>
#include <volumetric_fog.hpp>
using namespace kuki;
namespace {
/// @brief Materials the game draws with, staged next to the binary as `.mat` files.
///
/// Files rather than values built in code, because `AssetManager` owns asset identity and the
/// only public route into it is a load from a path. A handful of small JSON files is a fair price
/// for not reaching around the manager.
constexpr const char *MATERIALS[]{"piece_white", "piece_black", "piece_check", "piece_checkmate", "piece_draw", "piece_white_ghost", "piece_black_ghost"};
/// @brief Models the game draws with, one mesh apiece, exported from `src/model/chess.blend` at build
/// time to `.glb` files beside the binary. See `kuki_add_blender_models`.
///
/// Modelled in Blender to the board's own units -- a square is one unit, and every model stands on
/// its origin -- so each is drawn at unit scale and placed by where its base goes.
///
/// Each mesh is registered under its file's name, and a name lookup does not care what kind of
/// asset it finds -- so no material may share one. The clock's materials were once named after the
/// meshes they paint, and every part of the clock resolved to its material and was drawn as the
/// renderer's fallback cube. They are `clock_case_<theme>` and so on now; see `ClockMaterial`.
constexpr const char *MODELS[]{"pawn", "knight", "bishop", "rook", "queen", "king", "board", "clock_body", "clock_screen", "clock_rocker"};
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
  // A model loads as a prefab of nodes, which is the shape for instantiating a whole file into a
  // scene. The game wants something else: a mesh it can name on an entity it already has, and swap
  // when a square changes hands. So each file's one mesh is lifted out and registered on its own
  // under the file's name -- the route `GameManager` already takes for the meshes it builds itself.
  // The model keeps its own name with a suffix, so the two never answer to the same lookup.
  for (const auto *name : MODELS) {
    const auto modelId = LoadAsset<ModelAsset>(ResolvePath(std::string("model/") + name + ".glb"), std::string(name) + ".glb");
    const auto *model = modelId ? GetAsset<ModelAsset>(modelId) : nullptr;
    if (!model || model->meshes.empty()) {
      spdlog::warn("[Chess] Model {} did not load, so it will not be drawn", name);
      continue;
    }
    auto meshAsset = std::make_unique<MeshAsset>(MakeBuiltInAssetID(std::string("chess/model/") + name));
    meshAsset->mesh = model->meshes.front().mesh;
    meshAsset->bounds = BoundingBox::Calculate(meshAsset->mesh.vertices);
    AddAsset(std::move(meshAsset), name);
  }
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
    // The board is meant to read as being up in the sky, so it is given one to be up in. Present is
    // what switches the computed sky on: this scene had no skybox at all before, so what it stood
    // in front of was the flat background below, and the two settings after this are now what it
    // falls back to if the component is taken off again.
    //
    // Six kilometres, with the sun where the scene already put it. The altitude is the field this
    // was added for, and it is set well above the cloud layer below on purpose: the camera looks
    // down at the board, so what fills the frame past the board's edge is whatever is beneath it,
    // and the height is what turns that into a cloud sea seen from above rather than a wall of
    // cloud seen from inside.
    //
    // Everything else is left at Earth's values. The sky is only worth tuning once it is being
    // looked at, and the defaults are a clear afternoon.
    .With<AtmosphereSky>([](AtmosphereSky &sky) {
      sky.viewAltitude = 6.f;
    })
    // A cloud deck under the board rather than over it. The board is at six kilometres and the
    // camera looks down at it, so what fills the frame past the board's own edge is whatever is
    // below the horizon -- and a sea of cloud there is the thing that says how high up this is.
    // Clouds overhead would be almost entirely out of shot.
    //
    // Nine kilometres to a feature, which is far larger than the layer is thick and is set by how
    // much of the deck is in shot rather than by what a cloud is: the field tiles, and from this
    // height a smaller one repeats several times between the board and the horizon, which reads as
    // a woven pattern rather than as weather.
    //
    // The wind is slow enough that the deck reads as weather rather than as a moving texture: at
    // twelve metres a second a cloud takes a couple of minutes to cross the frame.
    .With<VolumetricClouds>([](VolumetricClouds &clouds) {
      clouds.bottomAltitude = 1.f;
      clouds.topAltitude = 2.6f;
      clouds.shapeScale = 9.f;
      clouds.detailScale = .8f;
      clouds.coverage = .5f;
      clouds.windSpeed = .012f;
    })
    // Thin. The board is in clear air two kilometres up, so almost nothing here is the scene's own
    // medium -- what the eye reads as distance is the aerial perspective, which is the atmosphere
    // above doing the same work for the air in front of the board that it already does for the sky
    // behind it.
    //
    // A hundredth of a kilometre to the unit puts the board at about eighty metres across, which is
    // the scale it looks. The shafts this leaves are faint and are meant to be: there is nothing
    // between the sun and the board to break them except the cloud, and when a cloud does cross the
    // sun the whole board dims with it.
    //
    // The density is what it is because a sky pixel marches this medium for the whole of
    // `maxDistance` before the sky is composited behind it. Sixty units of anything thicker than
    // this is a veil over the entire background, which is the first thing to check if the sky ever
    // comes out looking like weather it was not asked for.
    .With<VolumetricFog>([](VolumetricFog &fog) {
      fog.density = .003f;
      fog.heightFalloff = .08f;
      fog.lightScale = 1.2f;
      fog.maxDistance = 60.f;
      fog.kilometresPerUnit = .01f;
    })
    .With<IndirectLighting>([](IndirectLighting &lighting) {
      // Both of the settings below now shade nothing, and are kept for the case where the sky above
      // is removed. The fallback ambient applies only to a scene with no sky, and the background
      // colour only fills a frame that has nothing behind it -- so with the atmosphere present the
      // irradiance comes off the sky cubemap and the frame is filled by the sky itself.
      //
      // The flat fallback of 0.03 is tuned for a scene that has a sky, and leaves this board's
      // shadows black without one.
      lighting.ambientFallback = .18f;
      // Lighter than the engine's default, so the pieces -- one side near white, one near black --
      // both have something to stand out against. Darker and Black's disappear into it, lighter and
      // White's do.
      lighting.backgroundColor = {.34f, .35f, .4f};
    })
    .Entity("GameManager")
    .Script<GameManager>();
}
