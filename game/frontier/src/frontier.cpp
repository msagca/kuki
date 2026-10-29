#include <atmosphere_sky.hpp>
#include <camera.hpp>
#include <frontier.hpp>
#include <frontier_world.hpp>
#include <game_builder.hpp>
#include <game_module.hpp>
#include <glm/geometric.hpp>
#include <indirect_lighting.hpp>
#include <launch_path.hpp>
#include <light.hpp>
#include <light_type.hpp>
#include <planar_reflection.hpp>
#include <props.hpp>
#include <screen_effects.hpp>
#include <spdlog/spdlog.h>
#include <stylization.hpp>
#include <volumetric_clouds.hpp>
#include <volumetric_fog.hpp>
#include <wind.hpp>
using namespace kuki;
using namespace frontier;
Frontier::Frontier()
  : SystemApplication(ApplicationDescription{.name = "Kuki Frontier"}) {}
auto Frontier::Start() -> void {
  SetupFrontier(*this);
}
auto SetupFrontier(Application &app) -> void {
  LoadProps(app);
  if (!app.SetOverlayFont(ResolvePath(FRONTIER_FONT)))
    spdlog::warn("[Frontier] No font, so nothing will be written on screen");
  FrontierWorld::Game(app, "Kuki Frontier")
    .Scene("Main")
    .Entity<CameraObject>("Camera")
    .With<Camera>([](Camera &camera) {
      camera.fov = 38.f;
      camera.nearPlane = .3f;
      // Far enough for the open sea to reach the horizon. `CameraSystem` places it from here on.
      camera.farPlane = 900.f;
      camera.exposure = .1f;
    })
    .ActiveCamera()
    .Entity<LightObject>("Sun")
    .With<Light>([](Light &light) {
      light.type = LightType::Directional;
      light.forward = glm::normalize(glm::vec3(-.5f, -.6f, -.4f));
      light.diffuse = {1.f, .9f, .78f};
      light.intensity = 3.f;
      light.angularDiameter = 1.5f;
    })
    .Entity<Sky>("Sky")
    .With<AtmosphereSky>([](AtmosphereSky &sky) {
      sky.sunIntensity = 12.f;
      sky.sunDiscIntensity = .6f;
    })
    .With<IndirectLighting>([](IndirectLighting &lighting) {
      lighting.ambientFallback = .2f;
    })
    .Entity<Look>("Look")
    .With<Stylization>([](Stylization &look) {
      look.bands = 3;
      look.bandSoftness = .04f;
      look.shadowTint = {.6f, .62f, .9f};
      look.highlightSize = .04f;
      look.rimIntensity = .55f;
      look.inkThickness = 1.2f;
      // A map seen from high up has everything far away, so the lines fade only at the horizon.
      look.inkFadeDistance = 140.f;
      look.inkDepthThreshold = .045f;
      look.saturation = 1.18f;
      look.contrast = 1.05f;
      look.shadowColor = {.45f, .47f, .6f};
      look.highlightColor = {.56f, .53f, .47f};
      look.vignette = .25f;
    })
    .With<ScreenEffects>([](ScreenEffects &) {})
    // The sea is the one level surface, at nought, mirroring the land and the sky over it. Every
    // pixel within reach of a wave's crest of nought is taken for it; the beaches stand clear above.
    .With<PlanarReflection>([](PlanarReflection &water) {
      water.height = .0f;
      water.tolerance = .12f;
      water.strength = .3f;
      water.dryReflectance = 1.f;
      water.puddleCoverage = 1.f;
      water.blur = .5f;
      water.ripples = .0f;
      water.maxDistance = 80.f;
    })
    // A haze that thickens low over the water and gathers in the distance, so the land fades into
    // the sea at the horizon and the sun throws shafts through it.
    .Entity<VolumetricFogObject>("Air")
    .With<VolumetricFog>([](VolumetricFog &fog) {
      fog.density = .012f;
      fog.heightFalloff = .45f;
      fog.albedo = {.95f, .92f, .9f};
      fog.ambient = {.14f, .15f, .2f};
      fog.anisotropy = .55f;
      fog.lightScale = 1.6f;
      fog.maxDistance = 220.f;
      fog.aerialPerspective = 1.4f;
      fog.kilometresPerUnit = .02f;
    })
    .Entity<VolumetricCloudsObject>("Clouds")
    .With<VolumetricClouds>([](VolumetricClouds &clouds) {
      clouds.coverage = .42f;
      clouds.density = 9.f;
    })
    .Entity<WindObject>("Wind")
    .With<Wind>([](Wind &) {});
}
KUKI_GAME_MODULE("Frontier", SetupFrontier)
KUKI_GAME_MAIN(Frontier)
