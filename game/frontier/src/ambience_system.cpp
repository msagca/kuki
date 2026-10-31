#include <GLFW/glfw3.h>
#include <algorithm>
#include <ambience_system.hpp>
#include <application.hpp>
#include <atmosphere_sky.hpp>
#include <cmath>
#include <frontier_world.hpp>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>
#include <light.hpp>
#include <light_type.hpp>
#include <material_asset.hpp>
#include <noise.hpp>
#include <particle_emitter.hpp>
#include <props.hpp>
#include <scene.hpp>
#include <volumetric_clouds.hpp>
#include <wind.hpp>
using namespace kuki;
using namespace frontier;
namespace {
/// How hard the wind carries what drifts, in units a second squared for each unit of its strength.
constexpr float CARRY = .9f;
/// Seconds from dawn to dusk while the day is passing.
constexpr float DAY_LENGTH = 150.f;
auto Mix(const glm::vec3 &a, const glm::vec3 &b, const float t) -> glm::vec3 {
  return a + (b - a) * std::clamp(t, .0f, 1.f);
}
} // namespace
AmbienceSystem::AmbienceSystem(Application &app)
  : System(std::in_place_type<AmbienceSystem>, app) {}
auto AmbienceSystem::Update(const float deltaTime) -> void {
  const auto dt = std::min(deltaTime, .1f);
  clock += dt;
  if (app.IsInputPressed(GLFW_KEY_T))
    passing = !passing;
  if (passing) {
    // Dawn to dusk and back again, never through the night: the map is for looking at.
    day += dayDirection * dt / DAY_LENGTH;
    if (day > 1.f || day < .0f) {
      dayDirection = -dayDirection;
      day = std::clamp(day, .0f, 1.f);
    }
  }
  Weather();
  Sun();
}
auto AmbienceSystem::Weather() -> void {
  auto scene = app.GetScene();
  if (!scene)
    return;
  auto *wind = scene->GetAnyComponent<Wind>();
  if (!wind)
    return;
  // Turning through about a quarter of the compass over a minute or two, and rising and falling
  // faster than it turns, as wind does. A breeze and never a gale: at twice this the grass lay flat
  // under every gust and the field's heaving was all the eye went to, which is not what the map is
  // for. Everything the wind moves -- the sway, the drift, the birds, the sails -- is scaled by the
  // strength, so this one range calms all of it together.
  const auto heading = .5f + 1.4f * GradientNoise({clock * .012f, .5f}, 7u);
  const auto strength = std::clamp(.45f + .25f * FractalNoise({clock * .05f, 3.5f}, 3, 11u), .2f, .75f);
  wind->direction = {std::cos(heading), std::sin(heading)};
  wind->strength = strength;
  wind->gustStrength = .3f + .3f * strength;
  wind->flutterAmount = .2f;
  // Higher waves in a stronger wind, a little behind it, as a sea is.
  wind->waveHeight = .1f + .06f * strength;
  if (auto *clouds = scene->GetAnyComponent<VolumetricClouds>()) {
    clouds->windDirection = wind->direction;
    clouds->windSpeed = .008f + .008f * strength;
  }
  const glm::vec3 carry{wind->direction.x * strength * CARRY, .0f, wind->direction.y * strength * CARRY};
  // Every emitter whose particles wander is one the wind carries; the rings on the water and the
  // like have none, and are left as they are.
  FrontierWorld::ForEach<ParticleEmitter>(app, [&](const EntityID, ParticleEmitter *emitter) {
    if (emitter->turbulence <= .0f)
      return;
    emitter->gravity.x = carry.x;
    emitter->gravity.z = carry.z;
  });
  // The foam along the shore thickens and thins as the waves come in.
  if (auto *foam = app.GetAsset<MaterialAsset>(MaterialId("water/foam")))
    foam->fallback.albedo.w = .42f + .2f * std::sin(clock * 1.1f) + .08f * std::sin(clock * 2.9f + 1.f);
}
auto AmbienceSystem::Sun() -> void {
  auto scene = app.GetScene();
  if (!scene)
    return;
  Light *sun{};
  FrontierWorld::ForEach<Light>(app, [&](const EntityID, Light *light) {
    if (!sun && light->type == LightType::Directional)
      sun = light;
  });
  if (!sun)
    return;
  // Up from the east at dawn, highest at noon and down into the west at dusk.
  const auto elevation = glm::radians(3.f + 58.f * std::sin(day * 3.14159265f));
  const auto azimuth = glm::radians(-70.f + 140.f * day);
  const glm::vec3 towards{std::sin(azimuth) * std::cos(elevation), std::sin(elevation), -std::cos(azimuth) * std::cos(elevation)};
  sun->forward = -glm::normalize(towards);
  // Warm and weak near the horizon, white overhead.
  const auto height = std::sin(elevation);
  const auto colour = Mix({1.f, .5f, .28f}, {1.f, .95f, .88f}, height * 2.2f);
  sun->diffuse = colour;
  sun->specular = colour;
  sun->intensity = 1.2f + 2.4f * std::clamp(height * 2.f, .0f, 1.f);
  sun->ambient = Mix({.12f, .08f, .12f}, {.16f, .17f, .2f}, height * 2.f);
  if (auto *sky = scene->GetAnyComponent<AtmosphereSky>())
    sky->sunIntensity = 6.f + 8.f * std::clamp(height * 2.f, .0f, 1.f);
}
