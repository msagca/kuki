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
#include <model_asset.hpp>
#include <spdlog/spdlog.h>
using namespace kuki;
Duel::Duel()
  : SystemApplication(ApplicationDescription{.name = "Kuki Duel"}) {}
auto Duel::Start() -> void {
  SetupDuel(*this);
}
auto SetupDuel(Application &app) -> void {
  if (!app.LoadAsset<ModelAsset>(ResolvePath(std::string("model/") + FIGHTER_MODEL), FIGHTER_MODEL))
    spdlog::warn("[Duel] The fighter did not load, so there is nobody to play");
  if (!app.SetOverlayFont(ResolvePath(DUEL_FONT)))
    spdlog::warn("[Duel] No font, so nothing will be written on screen");
  duel::DuelWorld::Game(app, "Kuki Duel")
    .Scene("Main")
    .Entity<CameraObject>("Camera")
    .With<Camera>([](Camera &camera) {
      // From the side and a little above the waist, far enough back to keep both figures in frame
      // across the whole floor.
      camera.position = {.0f, 1.5f, 6.5f};
      camera.rotation = glm::quatLookAt(glm::normalize(glm::vec3(.0f, 1.f, .0f) - camera.position), glm::vec3(.0f, 1.f, .0f));
      camera.fov = 40.f;
      camera.farPlane = 80.f;
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
    .Entity<MeshObject>("Floor")
    .Mesh("Cube")
    .At({.0f, -.1f, .0f})
    .Scale({14.f, .2f, 5.f});
}
KUKI_GAME_MODULE("Duel", SetupDuel)
KUKI_GAME_MAIN(Duel)
