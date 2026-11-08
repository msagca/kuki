#include <ambient_occlusion.hpp>
#include <atmosphere_sky.hpp>
#include <camera.hpp>
#include <controls.hpp>
#include <depth_of_field.hpp>
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
#include <transform.hpp>
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
  RegisterControls(app);
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
    .Facing({-.5f, -.6f, -.4f})
    // A warm afternoon sun, wide in the sky so a shadow's edge blurs as it falls away from what
    // casts it: the soft light of a miniature rather than the hard one of a cel.
    .With<Light>([](Light &light) {
      light.type = LightType::Directional;
      light.diffuse = {1.f, .91f, .8f};
      light.intensity = 3.f;
      light.angularDiameter = 4.f;
    })
    .Entity<Sky>("Sky")
    .With<AtmosphereSky>([](AtmosphereSky &sky) {
      sky.sunIntensity = 12.f;
      sky.sunDiscIntensity = .6f;
    })
    // More of the sky into the shade, so a shadow is a gentle blue rather than a dark.
    .With<IndirectLighting>([](IndirectLighting &lighting) {
      lighting.skyIntensity = 1.3f;
      lighting.ambientFallback = .2f;
    })
    .Entity<Look>("Look")
    // Painted, not drawn: the frontier is meant to look apart from the duel's inked cels, soft and
    // calm like a tabletop model in daylight. No ink, since a line round every cell and roof is what
    // made the map read as a drawing; one band, as wide as it can be, so the light wraps round a form
    // in a gradient and the shaded side takes a cool tint without a step to it; no hard highlight or
    // bright rim; and a grade that warms the lights and lifts the darks a little towards blue. The
    // softness is in the colours and the light, not in the grade: turning saturation and contrast
    // below one as well washed the map out into a grey haze.
    .With<Stylization>([](Stylization &look) {
      look.bands = 2;
      look.bandSoftness = .4f;
      look.terminator = -.1f;
      look.shadowTint = {.8f, .82f, .96f};
      look.highlightSize = .0f;
      look.rimIntensity = .15f;
      look.rimWidth = .25f;
      look.inkThickness = .0f;
      look.saturation = 1.06f;
      look.contrast = .98f;
      look.shadowColor = {.51f, .51f, .56f};
      look.highlightColor = {.54f, .52f, .47f};
      look.vignette = .12f;
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
    // A soft shade where things meet: under the eaves and round the foot of a house, between the
    // trees, along the foot of a terrace's step. About the size of a cottage's doorway, so it settles
    // a prop onto its cell without darkening the cell, and a little of it in the sunlight too, which
    // is what a miniature photographed in daylight has where the probe field is too coarse to say.
    .With<AmbientOcclusion>([](AmbientOcclusion &occlusion) {
      occlusion.radius = .6f;
      occlusion.intensity = 1.6f;
      occlusion.power = 1.4f;
      occlusion.direct = .5f;
    })
    // The miniature's shallow focus, on whatever is in the middle of the picture: the band across it
    // stays sharp and the near cells at the bottom and the far shore at the top soften away.
    .With<DepthOfField>([](DepthOfField &focus) {
      focus.focusRange = .12f;
      focus.transition = .5f;
      focus.maxBlur = 6.f;
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
