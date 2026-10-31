#include <atmosphere_sky.hpp>
#include <camera.hpp>
#include <duel.hpp>
#include <duel_world.hpp>
#include <game_builder.hpp>
#include <game_module.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <indirect_lighting.hpp>
#include <launch_path.hpp>
#include <light.hpp>
#include <light_type.hpp>
#include <memory>
#include <model_asset.hpp>
#include <planar_reflection.hpp>
#include <roster.hpp>
#include <screen_effects.hpp>
#include <spdlog/spdlog.h>
#include <stage.hpp>
#include <stylization.hpp>
#include <unordered_map>
#include <volumetric_clouds.hpp>
#include <volumetric_fog.hpp>
using namespace kuki;
namespace {
/// @brief Loads one fighter of the roster and makes the copy a second player who picks the same
/// one is dressed in.
auto LoadFighter(Application &app, const duel::Character &character) -> void {
  const auto name = duel::ModelName(character);
  const auto id = app.LoadAsset<ModelAsset>(ResolvePath("model/" + name), name);
  auto *model = id ? app.GetAsset<ModelAsset>(id) : nullptr;
  if (!model) {
    spdlog::warn("[Duel] {} did not load, so there is nobody to pick under that name", character.name);
    return;
  }
  // Drawn as a cel, in the bands the scene's `Stylization` gives, whatever the stage behind is.
  for (auto &material : model->materials)
    material.fallback.toon = 1.f;
  // A copy under an id of its own, so the renderer keeps its materials apart. A second load of the
  // file would not do -- the asset manager hands back the first for the same path -- and a copy
  // shares nothing that one of the two could change under the other.
  auto alternate = std::make_unique<ModelAsset>(MakeBuiltInAssetID("duel/" + duel::ModelName(character, true)));
  alternate->materials = model->materials;
  alternate->meshes = model->meshes;
  alternate->nodes = model->nodes;
  alternate->textures = model->textures;
  alternate->animations = model->animations;
  // The materials are called `<Fighter>_<Part>`, and it is the part that says what to wear.
  const std::unordered_map<std::string, glm::vec3> colours{{"Jacket", character.jacket}, {"Trousers", character.trousers}, {"Band", character.band}, {"Gloves", character.gloves}};
  for (auto &material : alternate->materials) {
    material.resourceId = {};
    const auto part = material.name.substr(material.name.rfind('_') + 1);
    if (const auto it = colours.find(part); it != colours.end())
      material.fallback.albedo = glm::vec4(it->second, 1.f);
  }
  app.AddAsset(std::move(alternate), duel::ModelName(character, true));
}
} // namespace
Duel::Duel()
  : SystemApplication(ApplicationDescription{.name = "Kuki Duel"}) {}
auto Duel::Start() -> void {
  SetupDuel(*this);
}
auto SetupDuel(Application &app) -> void {
  for (const auto &character : duel::ROSTER)
    LoadFighter(app, character);
  if (!app.SetOverlayFont(ResolvePath(DUEL_FONT)))
    spdlog::warn("[Duel] No font, so nothing will be written on screen");
  // Every stage's parts, though only one stage stands at a time: switching is then a matter of
  // placing what is already loaded.
  duel::LoadStages(app);
  duel::DuelWorld::Game(app, "Kuki Duel")
    .Scene("Main")
    .Entity<CameraObject>("Camera")
    .With<Camera>([](Camera &camera) {
      // From the side and a little above the waist, far enough back to keep both figures in frame
      // across the whole floor.
      camera.position = {.0f, 1.5f, 6.5f};
      camera.rotation = glm::quatLookAt(glm::normalize(glm::vec3(.0f, 1.f, .0f) - camera.position), glm::vec3(.0f, 1.f, .0f));
      camera.fov = 40.f;
      // Far enough for the hills behind the temple and the towers behind the roof. `DuelSystem`
      // moves the camera from here on, following the fight.
      camera.farPlane = 400.f;
    })
    .ActiveCamera()
    .Entity<LightObject>("Sun")
    .With<Light>([](Light &light) {
      light.type = LightType::Directional;
      light.forward = glm::normalize(glm::vec3(-.4f, -1.f, -.55f));
      light.diffuse = {1.f, .96f, .9f};
      light.specular = {1.f, 1.f, 1.f};
      light.intensity = 3.f;
      // Wider than the sun, so the shadows of the limbs soften as they leave the feet.
      light.angularDiameter = 2.f;
    })
    .Entity<duel::Sky>("Sky")
    .With<AtmosphereSky>([](AtmosphereSky &) {})
    .With<IndirectLighting>([](IndirectLighting &lighting) {
      lighting.ambientFallback = .18f;
    })
    .Entity<duel::Look>("Look")
    .With<Stylization>([](Stylization &) {})
    .With<ScreenEffects>([](ScreenEffects &) {})
    .With<PlanarReflection>([](PlanarReflection &) {})
    // The air and the clouds, which every stage has and sets to suit itself. The ground and
    // everything on it is the stage's: `StageSystem` builds one once the fight is set up.
    .Entity<VolumetricFogObject>("Air")
    .With<VolumetricFog>([](VolumetricFog &) {})
    .Entity<VolumetricCloudsObject>("Clouds")
    .With<VolumetricClouds>([](VolumetricClouds &) {});
}
KUKI_GAME_MODULE("Duel", SetupDuel)
KUKI_GAME_MAIN(Duel)
