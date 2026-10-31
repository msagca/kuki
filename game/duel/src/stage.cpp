#include <application.hpp>
#include <atmosphere_sky.hpp>
#include <bounding_box.hpp>
#include <camera.hpp>
#include <duel_world.hpp>
#include <game_builder.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/trigonometric.hpp>
#include <indirect_lighting.hpp>
#include <launch_path.hpp>
#include <light.hpp>
#include <light_type.hpp>
#include <material_asset.hpp>
#include <memory>
#include <mesh_asset.hpp>
#include <model_asset.hpp>
#include <particle_emitter.hpp>
#include <particle_presets.hpp>
#include <planar_reflection.hpp>
#include <scene.hpp>
#include <spdlog/spdlog.h>
#include <stage.hpp>
#include <string>
#include <stylization.hpp>
#include <volumetric_clouds.hpp>
#include <volumetric_fog.hpp>
using namespace kuki;
namespace duel {
namespace {
  /// @brief One part of a stage and what it is made of: the mesh `make_stages.py` wrote under this
  /// name, and the surface the game paints it with. Colours are linear; an emissive above one blooms.
  ///
  /// `toon` is how far the part takes the cel bands. The figures take all of it. A stage takes less,
  /// and the further off a part is the less it takes, so the backdrop reads as painted behind figures
  /// that read as drawn.
  struct Surface {
    const char *name;
    glm::vec3 albedo;
    float roughness{.8f};
    glm::vec3 emissive{.0f};
    float metalness{.0f};
    float toon{.7f};
  };
  constexpr Surface SURFACES[]{
    // The alley. Everything near the ground is wet, and wet is dark and smooth.
    {"alley_asphalt", {.035f, .035f, .042f}, .55f, {}, .0f, .5f},
    {"alley_paint", {.55f, .52f, .42f}, .3f},
    {"alley_curb", {.2f, .2f, .21f}, .5f},
    {"alley_sidewalk", {.11f, .11f, .12f}, .3f},
    {"alley_wall_brick", {.2f, .075f, .055f}, .85f},
    {"alley_wall_teal", {.06f, .13f, .14f}, .85f},
    {"alley_wall_plaster", {.27f, .24f, .21f}, .9f},
    {"alley_trim", {.1f, .1f, .11f}},
    {"alley_shop_glow", {.9f, .8f, .6f}, .5f, {1.1f, .8f, .42f}},
    {"alley_frame", {.04f, .04f, .045f}, .4f},
    {"alley_door", {.09f, .06f, .045f}},
    {"alley_awning_red", {.42f, .035f, .045f}},
    {"alley_awning_blue", {.04f, .1f, .36f}},
    {"alley_awning_green", {.04f, .26f, .1f}},
    {"alley_windows_lit", {.9f, .7f, .4f}, .3f, {2.2f, 1.5f, .75f}},
    {"alley_windows_cool", {.5f, .7f, .9f}, .3f, {.9f, 1.4f, 2.2f}},
    {"alley_windows_dark", {.02f, .025f, .04f}, .1f},
    {"alley_sign_board", {.025f, .025f, .03f}, .5f},
    {"alley_neon_pink", {1.f, .3f, .7f}, .3f, {14.f, 1.6f, 7.f}},
    {"alley_neon_cyan", {.3f, .9f, 1.f}, .3f, {1.f, 9.f, 13.f}},
    {"alley_neon_amber", {1.f, .7f, .2f}, .3f, {13.f, 6.5f, .8f}},
    {"alley_metal", {.32f, .33f, .35f}, .45f, {}, .6f},
    {"alley_grille", {.06f, .06f, .07f}, .5f, {}, .5f},
    {"alley_pipe", {.22f, .22f, .24f}, .4f, {}, .7f},
    {"alley_iron", {.035f, .035f, .04f}, .5f, {}, .6f},
    {"alley_pole", {.13f, .12f, .11f}},
    {"alley_transformer", {.18f, .2f, .18f}, .6f, {}, .4f},
    {"alley_cable", {.01f, .01f, .012f}},
    {"alley_lamp", {1.f, .95f, .85f}, .3f, {9.f, 8.f, 6.f}},
    {"alley_vending_body", {.5f, .05f, .05f}, .35f},
    {"alley_vending_glow", {.9f, .95f, 1.f}, .3f, {4.f, 4.6f, 5.4f}},
    {"alley_bags", {.018f, .018f, .022f}, .25f},
    {"alley_bins", {.07f, .16f, .09f}, .6f},
    {"alley_tower", {.025f, .03f, .045f}, .9f, {}, .0f, .3f},
    {"alley_tower_glass", {.015f, .02f, .035f}, .15f, {}, .0f, .3f},
    {"alley_screen", {.4f, .8f, 1.f}, .3f, {1.6f, 5.f, 8.f}, .0f, .3f},
    // The temple, in a low sun.
    {"temple_gravel", {.4f, .36f, .3f}, .95f},
    {"temple_stone", {.4f, .38f, .35f}, .85f},
    {"temple_stone_dark", {.26f, .25f, .24f}, .85f},
    {"temple_vermilion", {.6f, .09f, .035f}, .55f},
    {"temple_plaster", {.76f, .72f, .64f}, .9f},
    {"temple_wood", {.2f, .11f, .055f}},
    {"temple_shoji", {.85f, .8f, .68f}, .9f, {.5f, .36f, .16f}},
    {"temple_roof_tile", {.07f, .08f, .1f}, .45f},
    {"temple_roof_ridge", {.045f, .045f, .055f}, .45f},
    {"temple_black", {.03f, .03f, .03f}, .5f},
    {"temple_lantern_glow", {.95f, .75f, .45f}, .6f, {6.f, 3.4f, 1.1f}},
    {"temple_bark", {.11f, .075f, .065f}},
    {"temple_blossom", {.95f, .6f, .7f}, .9f, {.06f, .02f, .03f}, .0f, .9f},
    {"temple_hills", {.28f, .3f, .42f}, 1.f, {}, .0f, .25f},
    {"temple_trees_far", {.1f, .14f, .1f}, 1.f, {}, .0f, .4f},
    // The roof, at dusk.
    {"roof_concrete", {.3f, .29f, .28f}, .9f},
    {"roof_seam", {.1f, .1f, .1f}},
    {"roof_parapet", {.38f, .36f, .34f}, .9f},
    {"roof_rail", {.2f, .21f, .22f}, .4f, {}, .7f},
    {"roof_steel", {.16f, .17f, .18f}, .5f, {}, .6f},
    {"roof_tank", {.3f, .18f, .1f}},
    {"roof_tank_roof", {.18f, .12f, .07f}},
    {"roof_metal", {.45f, .46f, .47f}, .5f, {}, .5f},
    {"roof_grille", {.05f, .05f, .06f}, .5f, {}, .5f},
    {"roof_door", {.13f, .18f, .23f}, .6f},
    {"roof_lamp", {1.f, .9f, .7f}, .4f, {6.f, 5.f, 3.4f}},
    {"roof_beacon", {1.f, .2f, .1f}, .3f, {16.f, .7f, .3f}},
    {"roof_billboard", {.9f, .85f, .8f}, .4f, {3.2f, 2.4f, 1.5f}},
    {"roof_building_near", {.18f, .17f, .18f}, .9f},
    {"roof_tower", {.09f, .1f, .14f}, .8f, {}, .0f, .35f},
    {"roof_tower_far", {.13f, .15f, .22f}, .9f, {}, .0f, .2f},
    {"roof_glass", {.04f, .05f, .08f}, .12f, {}, .0f, .3f},
    {"roof_windows_lit", {.9f, .7f, .4f}, .3f, {3.f, 2.f, 1.f}, .0f, .3f},
    {"roof_windows_cool", {.5f, .7f, .9f}, .3f, {1.1f, 1.7f, 2.7f}, .0f, .3f},
    // The shrine, in snow.
    {"shrine_snow", {.82f, .86f, .94f}, .85f, {}, .0f, .8f},
    {"shrine_stone", {.33f, .33f, .35f}, .85f},
    {"shrine_wood_dark", {.11f, .065f, .045f}},
    {"shrine_wood", {.28f, .16f, .075f}},
    {"shrine_plaster", {.78f, .76f, .7f}, .9f},
    {"shrine_shoji_lit", {.9f, .8f, .6f}, .8f, {2.6f, 1.7f, .75f}},
    {"shrine_roof", {.1f, .1f, .12f}, .5f},
    {"shrine_vermilion", {.58f, .075f, .035f}, .55f},
    {"shrine_rope", {.58f, .48f, .3f}},
    {"shrine_paper", {.95f, .95f, .92f}, .9f},
    {"shrine_lantern_glow", {.95f, .75f, .45f}, .6f, {6.f, 3.4f, 1.1f}},
    {"shrine_embers", {.3f, .1f, .02f}, .9f, {14.f, 4.5f, .9f}},
    {"shrine_bark", {.14f, .09f, .065f}},
    {"shrine_pine", {.035f, .1f, .07f}, .9f},
    {"shrine_rock", {.23f, .25f, .3f}, .95f, {}, .0f, .35f},
  };
  /// @brief The name a part's material is registered under. Not the mesh's own: an asset is looked up
  /// by name whatever kind it is, and the two answering to one name is how the chess clock was once
  /// drawn as the renderer's stand-in cube.
  auto SurfaceName(const std::string_view mesh) -> std::string {
    return std::string(mesh) + "_surface";
  }
  auto Prefix(const StageId id) -> std::string_view {
    switch (id) {
    case StageId::Alley:
      return "alley_";
    case StageId::Temple:
      return "temple_";
    case StageId::Rooftop:
      return "roof_";
    case StageId::Shrine:
      return "shrine_";
    }
    return "";
  }
  /// @brief A unit vector pointing where the light travels: down from `elevation` degrees above the
  /// horizon, from `azimuth` degrees round from straight behind the fighters towards the right.
  auto SunTravel(const float elevation, const float azimuth) -> glm::vec3 {
    const auto e = glm::radians(elevation);
    const auto a = glm::radians(azimuth);
    // From behind the fighters is from -Z; the light then travels towards +Z, down and across.
    return -glm::normalize(glm::vec3(std::sin(a) * std::cos(e), std::sin(e), -std::cos(a) * std::cos(e)));
  }
  template <typename T>
  auto Singleton(Application &app) -> T * {
    auto scene = app.GetScene();
    return scene ? scene->GetAnyComponent<T>() : nullptr;
  }
  auto Sun(Application &app) -> Light * {
    Light *sun{};
    DuelWorld::ForEach<Light>(app, [&](const EntityID, Light *light) {
      if (!sun && light->type == LightType::Directional)
        sun = light;
    });
    return sun;
  }
  /// @brief A lamp: a point light at a place, of a colour and a strength, that falls off over about
  /// `reach` units.
  auto PointLight(Application &app, StageScene &stage, const char *name, const glm::vec3 &position, const glm::vec3 &colour, const float intensity, const float reach, const StageLight::Kind kind = StageLight::Kind::Steady) -> EntityID {
    const auto id = DuelWorld::Game(app)
                      .Scene("Main")
                      .Entity<LightObject>(name)
                      .With<Light>([&](Light &light) {
                        light.type = LightType::Point;
                        light.position = position;
                        light.diffuse = colour;
                        light.specular = colour;
                        light.intensity = intensity;
                        light.constant = 1.f;
                        light.linear = 2.f / reach;
                        light.quadratic = 4.f / (reach * reach);
                      })
                      .Id();
    stage.entities.push_back(id);
    stage.lights.push_back({id, kind, intensity});
    return id;
  }
  auto SpotLight(Application &app, StageScene &stage, const char *name, const glm::vec3 &position, const glm::vec3 &direction, const glm::vec3 &colour, const float intensity, const float reach, const float inner, const float outer) -> EntityID {
    const auto id = DuelWorld::Game(app)
                      .Scene("Main")
                      .Entity<LightObject>(name)
                      .With<Light>([&](Light &light) {
                        light.type = LightType::Spot;
                        light.position = position;
                        light.forward = glm::normalize(direction);
                        light.diffuse = colour;
                        light.specular = colour;
                        light.intensity = intensity;
                        light.constant = 1.f;
                        light.linear = 2.f / reach;
                        light.quadratic = 4.f / (reach * reach);
                        light.innerCutoff = std::cos(glm::radians(inner));
                        light.outerCutoff = std::cos(glm::radians(outer));
                      })
                      .Id();
    stage.entities.push_back(id);
    stage.lights.push_back({id, StageLight::Kind::Steady, intensity});
    return id;
  }
  /// @brief An emitter, set up by `configure`, at a place and turned by a rotation.
  auto Emitter(Application &app, StageScene &stage, const char *name, const glm::vec3 &position, const glm::quat &rotation, auto &&configure) -> EntityID {
    const auto id = DuelWorld::Game(app).Scene("Main").Entity<ParticleObject>(name).With<ParticleEmitter>(configure).At(position).Rotate(rotation).Id();
    stage.entities.push_back(id);
    return id;
  }
  /// @brief Turned upside down, so an emitter's +Y -- the way it throws -- points at the ground,
  /// leaning `lean` degrees off straight down as wind would push it.
  auto Downwards(const float lean = .0f) -> glm::quat {
    return glm::angleAxis(glm::radians(180.f), glm::vec3(1.f, .0f, .0f)) * glm::angleAxis(glm::radians(lean), glm::vec3(.0f, .0f, 1.f));
  }
  /// @brief What every stage sets on the scene's shared entities, in one place so no stage forgets one.
  struct Air {
    float sunElevation{};
    float sunAzimuth{};
    glm::vec3 sunColour{1.f};
    float sunIntensity{3.f};
    glm::vec3 sunAmbient{.2f};
    float skyIntensity{10.f};
    float sunDisc{1.f};
    float exposure{.0f};
    float ambientFallback{.18f};
    VolumetricFog fog{};
    VolumetricClouds clouds{};
    Stylization look{};
    /// Dry by default: a stage that wants the ground to shine says so.
    PlanarReflection ground{};
  };
  auto ApplyAir(Application &app, StageScene &stage, const Air &air) -> void {
    if (auto *sun = Sun(app)) {
      sun->forward = SunTravel(air.sunElevation, air.sunAzimuth);
      sun->diffuse = air.sunColour;
      sun->specular = air.sunColour;
      sun->intensity = air.sunIntensity;
      sun->ambient = air.sunAmbient;
      stage.sunColour = air.sunColour;
      stage.sunIntensity = air.sunIntensity;
    }
    if (auto *sky = Singleton<AtmosphereSky>(app)) {
      sky->sunIntensity = air.skyIntensity;
      sky->sunDiscIntensity = air.sunDisc;
    }
    if (auto *lighting = Singleton<IndirectLighting>(app))
      lighting->ambientFallback = air.ambientFallback;
    if (auto *fog = Singleton<VolumetricFog>(app))
      *fog = air.fog;
    if (auto *clouds = Singleton<VolumetricClouds>(app))
      *clouds = air.clouds;
    if (auto *look = Singleton<Stylization>(app))
      *look = air.look;
    if (auto *ground = Singleton<PlanarReflection>(app))
      *ground = air.ground;
    if (auto scene = app.GetScene())
      if (auto *camera = scene->GetCamera())
        camera->exposure = air.exposure;
  }
  auto BuildAlley(Application &app, StageScene &stage) -> void {
    stage.storm = true;
    Air air;
    // A moon rather than a sun: high, cold and faint. The sky is the atmosphere with almost nothing
    // lighting it, which leaves a deep blue rather than black, and the clouds close over it.
    air.sunElevation = 52.f;
    air.sunAzimuth = -35.f;
    air.sunColour = {.5f, .62f, 1.f};
    air.sunIntensity = .55f;
    air.sunAmbient = {.03f, .03f, .06f};
    air.skyIntensity = .35f;
    air.sunDisc = .0f;
    air.exposure = -.4f;
    air.ambientFallback = .05f;
    air.fog.density = .022f;
    air.fog.heightFalloff = .15f;
    air.fog.albedo = {.7f, .75f, .95f};
    air.fog.ambient = {.006f, .005f, .014f};
    air.fog.lightScale = 1.4f;
    air.fog.maxDistance = 70.f;
    air.fog.aerialPerspective = .0f;
    air.clouds.coverage = .85f;
    air.clouds.density = 20.f;
    air.clouds.ambient = .35f;
    air.clouds.bottomAltitude = .6f;
    air.clouds.topAltitude = 2.4f;
    air.look.shadowTint = {.55f, .5f, .9f};
    air.look.saturation = 1.25f;
    air.look.contrast = 1.16f;
    air.look.shadowColor = {.42f, .43f, .62f};
    air.look.highlightColor = {.56f, .5f, .5f};
    air.look.vignette = .38f;
    air.look.inkFadeDistance = 26.f;
    air.look.rimIntensity = .9f;
    // Soaked: puddles over half the street, the rest of it still shining, and rain in all of it.
    air.ground.strength = .8f;
    air.ground.dryReflectance = .22f;
    air.ground.puddleCoverage = .45f;
    air.ground.puddleScale = .32f;
    air.ground.blur = .45f;
    air.ground.ripples = 1.f;
    air.ground.rippleDensity = 2.2f;
    air.ground.maxDistance = 40.f;
    ApplyAir(app, stage, air);
    // The neon spills onto the street and the figures: pink, cyan and amber along the shopfronts, a
    // cold lamp over the kerb, the glow of the vending machines. One sign stutters.
    PointLight(app, stage, "Neon Pink", {-12.f, 4.2f, -5.4f}, {1.f, .2f, .6f}, 7.f, 9.f);
    PointLight(app, stage, "Neon Cyan", {-3.5f, 3.8f, -5.4f}, {.2f, .8f, 1.f}, 6.f, 9.f, StageLight::Kind::Stutter);
    PointLight(app, stage, "Neon Amber", {5.5f, 3.6f, -5.4f}, {1.f, .55f, .12f}, 6.f, 8.f);
    PointLight(app, stage, "Neon Pink 2", {14.f, 4.4f, -5.4f}, {1.f, .25f, .7f}, 7.f, 9.f);
    PointLight(app, stage, "Vending", {-3.f, 1.2f, -4.6f}, {.75f, .85f, 1.f}, 3.f, 4.f);
    PointLight(app, stage, "Shop", {-19.f, 1.5f, -5.6f}, {1.f, .7f, .35f}, 3.f, 6.f);
    // A shop on this side of the street, out of shot, whose window lights the figures from the front.
    PointLight(app, stage, "Shop Behind", {1.5f, 2.6f, 5.5f}, {1.f, .62f, .35f}, 4.5f, 11.f);
    SpotLight(app, stage, "Street Lamp", {6.5f, 5.55f, -3.1f}, {-.15f, -1.f, .25f}, {.85f, .9f, 1.f}, 26.f, 12.f, 28.f, 46.f);
    // The rain: streaks over the whole street, a splash where each would land, rings spreading in the
    // puddles, and a breath of steam from the grate.
    // Kept well short of the camera: a drop that falls past the lens is drawn the size of the screen.
    Emitter(app, stage, "Rain", {.0f, 8.5f, -3.f}, Downwards(8.f), [](ParticleEmitter &rain) {
      rain.shape = ParticleShape::Box;
      rain.extent = {17.f, 4.5f, 5.5f};
      rain.capacity = 16384;
      rain.rate = 9000.f;
      rain.lifetime = 1.35f;
      rain.lifetimeVariance = .1f;
      rain.speed = 11.f;
      rain.speedVariance = .15f;
      rain.spread = 2.f;
      rain.gravity = {.0f, -6.f, .0f};
      rain.drag = .0f;
      rain.facing = ParticleFacing::Velocity;
      rain.stretch = .03f;
      rain.startSize = .011f;
      rain.endSize = .011f;
      rain.startColor = {.55f, .62f, .8f};
      rain.endColor = {.55f, .62f, .8f};
      rain.intensity = .9f;
    });
    Emitter(app, stage, "Splashes", {.0f, .02f, -2.f}, glm::quat(1.f, .0f, .0f, .0f), [](ParticleEmitter &splash) {
      splash.shape = ParticleShape::Box;
      splash.extent = {16.f, .0f, 5.f};
      splash.capacity = 2048;
      splash.rate = 2600.f;
      splash.lifetime = .28f;
      splash.speed = 1.4f;
      splash.speedVariance = .6f;
      splash.spread = 40.f;
      splash.gravity = {.0f, -9.81f, .0f};
      splash.drag = .0f;
      splash.facing = ParticleFacing::Velocity;
      splash.stretch = .02f;
      splash.startSize = .014f;
      splash.endSize = .006f;
      splash.startColor = {.6f, .68f, .85f};
      splash.endColor = {.6f, .68f, .85f};
      splash.intensity = 1.1f;
    });
    Emitter(app, stage, "Ripples", {.0f, .01f, -1.f}, glm::quat(1.f, .0f, .0f, .0f), [](ParticleEmitter &ripples) {
      ripples.shape = ParticleShape::Box;
      ripples.extent = {14.f, .0f, 3.5f};
      ripples.capacity = 1024;
      ripples.rate = 700.f;
      ripples.lifetime = .6f;
      ripples.lifetimeVariance = .3f;
      ripples.speed = .0f;
      ripples.gravity = {};
      ripples.facing = ParticleFacing::Flat;
      ripples.sprite = ParticleSprite::Ring;
      ripples.startSize = .04f;
      ripples.endSize = .38f;
      ripples.startColor = {.5f, .58f, .75f};
      ripples.endColor = {.5f, .58f, .75f};
      ripples.intensity = .55f;
    });
    Emitter(app, stage, "Steam", {-5.f, .05f, -1.f}, glm::quat(1.f, .0f, .0f, .0f), [](ParticleEmitter &steam) {
      steam.capacity = 256;
      steam.rate = 22.f;
      steam.blend = ParticleBlend::Alpha;
      steam.opacity = .22f;
      steam.radius = .35f;
      steam.lifetime = 3.5f;
      steam.speed = .7f;
      steam.spread = 15.f;
      steam.gravity = {.25f, .35f, .0f};
      steam.drag = .5f;
      steam.turbulence = 1.2f;
      steam.startSize = .35f;
      steam.endSize = 1.9f;
      steam.startColor = {.5f, .5f, .62f};
      steam.endColor = {.3f, .3f, .4f};
      steam.intensity = 1.f;
    });
  }
  auto BuildTemple(Application &app, StageScene &stage) -> void {
    Air air;
    // Low behind the gate and a little to the left, so the light comes through the gate towards the
    // camera: rims on the figures, long shadows towards us, shafts through the haze.
    air.sunElevation = 9.f;
    air.sunAzimuth = -22.f;
    air.sunColour = {1.f, .6f, .32f};
    air.sunIntensity = 3.4f;
    air.sunAmbient = {.12f, .08f, .1f};
    air.skyIntensity = 12.f;
    air.exposure = .2f;
    air.fog.density = .018f;
    air.fog.heightFalloff = .18f;
    air.fog.albedo = {1.f, .9f, .82f};
    air.fog.ambient = {.12f, .08f, .1f};
    air.fog.anisotropy = .6f;
    air.fog.lightScale = 2.2f;
    air.fog.maxDistance = 90.f;
    air.fog.aerialPerspective = 1.4f;
    air.fog.kilometresPerUnit = .02f;
    air.clouds.coverage = .32f;
    air.clouds.density = 8.f;
    air.look.shadowTint = {.7f, .55f, .85f};
    air.look.saturation = 1.15f;
    air.look.shadowColor = {.46f, .44f, .58f};
    air.look.highlightColor = {.58f, .52f, .44f};
    air.look.vignette = .28f;
    air.look.rimIntensity = 1.2f;
    air.look.inkFadeDistance = 34.f;
    ApplyAir(app, stage, air);
    for (const auto x : {-11.f, -5.5f, 5.5f, 11.f})
      PointLight(app, stage, "Lantern", {x, 1.82f, -3.2f}, {1.f, .6f, .25f}, 2.4f, 3.5f, StageLight::Kind::Flicker);
    // Petals falling and wandering, and dust lit gold where the sun catches it.
    Emitter(app, stage, "Petals", {.0f, 6.f, -3.f}, glm::quat(1.f, .0f, .0f, .0f), [](ParticleEmitter &petals) {
      petals.shape = ParticleShape::Box;
      petals.extent = {16.f, 3.f, 6.f};
      petals.capacity = 1024;
      petals.rate = 80.f;
      petals.lifetime = 10.f;
      petals.lifetimeVariance = .2f;
      petals.speed = .2f;
      petals.spread = 180.f;
      petals.gravity = {.55f, -.55f, .0f};
      petals.drag = 1.1f;
      petals.turbulence = 2.6f;
      petals.blend = ParticleBlend::Opaque;
      petals.sprite = ParticleSprite::Petal;
      petals.startSize = .11f;
      petals.endSize = .1f;
      petals.startColor = {1.f, .32f, .5f};
      petals.endColor = {.9f, .3f, .46f};
      petals.intensity = .75f;
    });
    Emitter(app, stage, "Motes", {.0f, 1.8f, -2.f}, glm::quat(1.f, .0f, .0f, .0f), [](ParticleEmitter &motes) {
      motes.shape = ParticleShape::Box;
      motes.extent = {10.f, 1.6f, 4.f};
      motes.capacity = 512;
      motes.rate = 40.f;
      motes.lifetime = 7.f;
      motes.speed = .05f;
      motes.spread = 180.f;
      motes.gravity = {.05f, .02f, .0f};
      motes.turbulence = .4f;
      motes.startSize = .025f;
      motes.endSize = .02f;
      motes.startColor = {1.f, .78f, .45f};
      motes.endColor = {1.f, .6f, .3f};
      motes.intensity = 2.5f;
    });
  }
  auto BuildRooftop(Application &app, StageScene &stage) -> void {
    Air air;
    // The sun just down behind the city on the right: a sky going from orange to violet, the clouds
    // lit from underneath, and every window coming on.
    air.sunElevation = 2.5f;
    air.sunAzimuth = 55.f;
    air.sunColour = {1.f, .45f, .3f};
    air.sunIntensity = 1.6f;
    air.sunAmbient = {.08f, .06f, .12f};
    air.skyIntensity = 9.f;
    air.exposure = .9f;
    air.fog.density = .012f;
    air.fog.heightFalloff = .05f;
    air.fog.albedo = {.95f, .85f, .95f};
    air.fog.ambient = {.08f, .06f, .12f};
    air.fog.anisotropy = .5f;
    air.fog.lightScale = 1.5f;
    air.fog.maxDistance = 140.f;
    air.fog.aerialPerspective = 1.6f;
    air.fog.kilometresPerUnit = .015f;
    air.clouds.coverage = .5f;
    air.clouds.density = 10.f;
    air.clouds.windSpeed = .03f;
    air.look.shadowTint = {.55f, .5f, .92f};
    air.look.saturation = 1.18f;
    air.look.shadowColor = {.45f, .45f, .62f};
    air.look.highlightColor = {.6f, .5f, .46f};
    air.look.vignette = .3f;
    air.look.rimIntensity = 1.3f;
    air.look.inkFadeDistance = 30.f;
    ApplyAir(app, stage, air);
    PointLight(app, stage, "Beacon", {3.5f, 12.2f, -6.8f}, {1.f, .1f, .05f}, 6.f, 5.f, StageLight::Kind::Blink);
    PointLight(app, stage, "Door Lamp", {14.5f, 2.4f, -2.8f}, {1.f, .8f, .55f}, 3.f, 5.f, StageLight::Kind::Stutter);
    SpotLight(app, stage, "Billboard", {-3.5f, 3.6f, -9.f}, {.0f, .55f, -1.f}, {1.f, .85f, .6f}, 22.f, 9.f, 30.f, 50.f);
    PointLight(app, stage, "City Glow", {.0f, 1.f, -9.f}, {.45f, .4f, .9f}, 2.5f, 14.f);
    // Litter carried off the roof by the wind, and a glow round the beacon when it lights.
    Emitter(app, stage, "Litter", {-2.f, 1.5f, -1.5f}, glm::quat(1.f, .0f, .0f, .0f), [](ParticleEmitter &litter) {
      litter.shape = ParticleShape::Box;
      litter.extent = {14.f, 1.5f, 5.f};
      litter.capacity = 256;
      litter.rate = 14.f;
      litter.lifetime = 6.f;
      litter.speed = .4f;
      litter.spread = 180.f;
      litter.gravity = {3.2f, -.3f, .0f};
      litter.drag = .7f;
      litter.turbulence = 5.f;
      litter.blend = ParticleBlend::Opaque;
      litter.sprite = ParticleSprite::Petal;
      litter.startSize = .12f;
      litter.endSize = .1f;
      litter.startColor = {.75f, .7f, .62f};
      litter.endColor = {.6f, .56f, .5f};
      litter.intensity = .6f;
    });
    stage.blinkGlow = Emitter(app, stage, "Beacon Glow", {3.5f, 12.2f, -6.8f}, glm::quat(1.f, .0f, .0f, .0f), [](ParticleEmitter &glow) {
      glow.capacity = 4;
      glow.emitting = false;
      glow.burst = 1;
      glow.radius = .0f;
      glow.lifetime = .7f;
      glow.lifetimeVariance = .0f;
      glow.speed = .0f;
      glow.gravity = {};
      glow.startSize = 1.6f;
      glow.endSize = 1.2f;
      glow.startColor = {1.f, .15f, .08f};
      glow.endColor = {1.f, .1f, .05f};
      glow.intensity = 3.f;
    });
  }
  auto BuildShrine(Application &app, StageScene &stage) -> void {
    Air air;
    // A low, pale sun through cloud, the whole scene blue in the shade and warm only by the fire.
    air.sunElevation = 18.f;
    air.sunAzimuth = 30.f;
    air.sunColour = {.9f, .92f, 1.f};
    air.sunIntensity = 1.5f;
    air.sunAmbient = {.15f, .17f, .22f};
    air.skyIntensity = 7.f;
    air.sunDisc = .3f;
    air.exposure = -.1f;
    air.fog.density = .04f;
    air.fog.heightFalloff = .1f;
    air.fog.albedo = {.92f, .95f, 1.f};
    air.fog.ambient = {.2f, .23f, .3f};
    air.fog.lightScale = 1.2f;
    air.fog.maxDistance = 90.f;
    air.fog.aerialPerspective = 1.2f;
    air.fog.kilometresPerUnit = .02f;
    air.clouds.coverage = .68f;
    air.clouds.density = 14.f;
    air.look.shadowTint = {.6f, .68f, .95f};
    air.look.saturation = 1.05f;
    air.look.shadowColor = {.44f, .48f, .6f};
    air.look.highlightColor = {.52f, .52f, .5f};
    air.look.vignette = .3f;
    air.look.inkFadeDistance = 32.f;
    ApplyAir(app, stage, air);
    PointLight(app, stage, "Fire", {-7.5f, .8f, -2.f}, {1.f, .45f, .12f}, 9.f, 7.f, StageLight::Kind::Flicker);
    for (const auto x : {-6.f, 6.f})
      PointLight(app, stage, "Lantern", {x, 1.82f, -4.f}, {1.f, .6f, .25f}, 2.f, 3.f, StageLight::Kind::Flicker);
    PointLight(app, stage, "Hall", {.0f, 2.4f, -13.f}, {1.f, .7f, .35f}, 4.f, 6.f);
    // Short of the camera, as the rain is, so no flake is drawn the size of the lens.
    Emitter(app, stage, "Snow", {.0f, 7.f, -4.5f}, Downwards(10.f), [](ParticleEmitter &snow) {
      snow.shape = ParticleShape::Box;
      snow.extent = {18.f, 4.5f, 6.f};
      snow.capacity = 16384;
      snow.rate = 1100.f;
      snow.lifetime = 12.f;
      snow.lifetimeVariance = .2f;
      snow.speed = .8f;
      snow.spread = 10.f;
      snow.gravity = {.0f, -.15f, .0f};
      snow.drag = .6f;
      snow.turbulence = 1.3f;
      snow.blend = ParticleBlend::Opaque;
      snow.startSize = .035f;
      snow.endSize = .03f;
      snow.startColor = {.95f, .97f, 1.f};
      snow.endColor = {.95f, .97f, 1.f};
      snow.intensity = 1.f;
    });
    Emitter(app, stage, "Embers", {-7.5f, .4f, -2.f}, glm::quat(1.f, .0f, .0f, .0f), [](ParticleEmitter &embers) {
      embers = ParticlePresets::Embers();
      embers.rate = 45.f;
      embers.radius = .35f;
      embers.speed = 1.2f;
      embers.gravity = {.15f, 1.1f, .0f};
      embers.turbulence = 2.f;
      embers.intensity = 6.f;
    });
    Emitter(app, stage, "Smoke", {-7.5f, .9f, -2.f}, glm::quat(1.f, .0f, .0f, .0f), [](ParticleEmitter &smoke) {
      smoke.capacity = 256;
      smoke.rate = 14.f;
      smoke.blend = ParticleBlend::Alpha;
      smoke.opacity = .3f;
      smoke.radius = .25f;
      smoke.lifetime = 5.f;
      smoke.speed = .8f;
      smoke.spread = 12.f;
      smoke.gravity = {.3f, .4f, .0f};
      smoke.drag = .4f;
      smoke.turbulence = .9f;
      smoke.startSize = .4f;
      smoke.endSize = 2.6f;
      smoke.startColor = {.3f, .3f, .32f};
      smoke.endColor = {.5f, .52f, .56f};
      smoke.intensity = 1.f;
    });
  }
} // namespace
auto FindStage(const std::string_view name) -> std::optional<StageId> {
  for (size_t i = 0; i < STAGE_COUNT; ++i)
    if (name == STAGE_NAMES[i])
      return static_cast<StageId>(i);
  return std::nullopt;
}
auto LoadStages(Application &app) -> void {
  auto loaded = 0;
  for (const auto &surface : SURFACES) {
    // Each part is a file of one mesh, lifted out of the model it loads as and registered under the
    // part's own name, as the chess pieces are. The model keeps the file's name, which is not the
    // part's, so the two never answer to one lookup.
    const auto file = std::string(surface.name) + ".glb";
    const auto modelId = app.LoadAsset<ModelAsset>(ResolvePath("stage/" + file), file);
    const auto *model = modelId ? app.GetAsset<ModelAsset>(modelId) : nullptr;
    if (!model || model->meshes.empty()) {
      spdlog::warn("[Duel] Stage part {} did not load, so it will not be drawn", surface.name);
      continue;
    }
    auto mesh = std::make_unique<MeshAsset>(MakeBuiltInAssetID(std::string("duel/stage/") + surface.name));
    mesh->mesh = model->meshes.front().mesh;
    mesh->bounds = BoundingBox::Calculate(mesh->mesh.vertices);
    app.AddAsset(std::move(mesh), surface.name);
    auto material = std::make_unique<MaterialAsset>(MakeBuiltInAssetID(std::string("duel/stage/surface/") + surface.name));
    material->type = MaterialType::Lit;
    material->fallback.albedo = glm::vec4(surface.albedo, 1.f);
    material->fallback.emissive = glm::vec4(surface.emissive, 1.f);
    material->fallback.roughness = surface.roughness;
    material->fallback.metalness = surface.metalness;
    material->fallback.specular = glm::vec4(1.f);
    material->fallback.toon = surface.toon;
    app.AddAsset(std::move(material), SurfaceName(surface.name));
    ++loaded;
  }
  spdlog::info("[Duel] Loaded {} stage parts", loaded);
}
auto BuildStage(Application &app, const StageId id) -> StageScene {
  StageScene stage{.id = id};
  const auto prefix = Prefix(id);
  for (const auto &surface : SURFACES) {
    const std::string_view name = surface.name;
    if (!name.starts_with(prefix) || !app.GetAsset(std::string(name)))
      continue;
    // Where it was built: the parts were modelled in place round the origin, so they stand where
    // they belong with no transform of their own.
    stage.entities.push_back(DuelWorld::Game(app).Scene("Main").Entity<MeshObject>(std::string(name)).Mesh(std::string(name)).Material(SurfaceName(name)).Id());
  }
  switch (id) {
  case StageId::Alley:
    BuildAlley(app, stage);
    break;
  case StageId::Temple:
    BuildTemple(app, stage);
    break;
  case StageId::Rooftop:
    BuildRooftop(app, stage);
    break;
  case StageId::Shrine:
    BuildShrine(app, stage);
    break;
  }
  spdlog::info("[Duel] Built the {} stage from {} entities", STAGE_NAMES[static_cast<size_t>(id)], stage.entities.size());
  return stage;
}
auto ClearStage(Application &app, StageScene &stage) -> void {
  for (const auto id : stage.entities)
    app.DeleteEntity(id);
  stage.entities.clear();
  stage.lights.clear();
  stage.blinkGlow = {};
}
} // namespace duel
