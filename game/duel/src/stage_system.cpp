#include <GLFW/glfw3.h>
#include <algorithm>
#include <application.hpp>
#include <cmath>
#include <command_line.hpp>
#include <duel_system.hpp>
#include <duel_world.hpp>
#include <light.hpp>
#include <particle_emitter.hpp>
#include <scene.hpp>
#include <screen_effects.hpp>
#include <spdlog/spdlog.h>
#include <stage.hpp>
#include <stage_system.hpp>
using namespace kuki;
using duel::DuelWorld;
using duel::StageLight;
namespace {
/// How long a bolt lasts, from the first flicker to the last, in seconds; and how long to wait
/// between them, at the least and at the most.
constexpr float STRIKE_LENGTH = .7f;
constexpr float STRIKE_GAP_MIN = 5.f;
constexpr float STRIKE_GAP_MAX = 13.f;
/// How bright the moon becomes while a bolt lights the street, and how white the screen goes.
constexpr float STRIKE_SUN = 9.f;
constexpr float STRIKE_FLASH = .22f;
/// A bolt is three flickers, at these moments and for these lengths, the first the strongest.
constexpr float FLICKER_AT[]{.0f, .14f, .32f};
constexpr float FLICKER_FOR[]{.07f, .05f, .16f};
constexpr float FLICKER_STRENGTH[]{1.f, .55f, .8f};
} // namespace
StageSystem::StageSystem(Application &app)
  : System(std::in_place_type<StageSystem>, app) {}
auto StageSystem::Random() -> float {
  seed ^= seed << 13;
  seed ^= seed >> 17;
  seed ^= seed << 5;
  return static_cast<float>(seed >> 8) / 16777216.f;
}
auto StageSystem::PostStart() -> void {
  auto chosen = duel::StageId::Alley;
  if (const auto name = GetFlagValue(GetCommandLineArguments(), STAGE_FLAG); name) {
    if (const auto found = duel::FindStage(*name); found)
      chosen = *found;
    else
      spdlog::warn("[Duel] There is no stage called {}, so the fight is in the alley", *name);
  }
  Build(chosen);
  // The lightning lights the street either way; it is only the white over the whole screen that
  // the duel's `--no-flash` takes away.
  flashing = !HasFlag(GetCommandLineArguments(), DuelSystem::NO_FLASH_FLAG);
  // Asked for here and carried out in `Update`: the stage is taken down entity by entity, and the
  // middle of the input manager's dispatch is no place to be deleting things from the scene.
  nextAction = app.RegisterInputAction(GLFW_KEY_TAB, [this]() {
    switchPending = true;
  });
  spdlog::info("[Duel] Tab moves the fight to the next stage");
}
auto StageSystem::Shutdown() -> void {
  app.UnregisterInputAction(nextAction);
}
auto StageSystem::Build(const duel::StageId id) -> void {
  duel::ClearStage(app, stage);
  stage = duel::BuildStage(app, id);
  clock = .0f;
  strike = -1.f;
  nextStrike = STRIKE_GAP_MIN * .5f;
}
auto StageSystem::Update(const float deltaTime) -> void {
  if (switchPending) {
    switchPending = false;
    Build(static_cast<duel::StageId>((static_cast<size_t>(stage.id) + 1) % duel::STAGE_COUNT));
  }
  clock += deltaTime;
  AnimateLights();
  if (stage.storm)
    Storm(deltaTime);
}
auto StageSystem::AnimateLights() -> void {
  // Every light is a function of the clock and of its own place in the list, so no two fires
  // flicker together and nothing needs remembering from one frame to the next.
  auto beaconOn = false;
  for (size_t i = 0; i < stage.lights.size(); ++i) {
    const auto &entry = stage.lights[i];
    auto *light = DuelWorld::Get<Light>(app, entry.entity);
    if (!light)
      continue;
    const auto t = clock + static_cast<float>(i) * 1.7f;
    auto scale = 1.f;
    switch (entry.kind) {
    case StageLight::Kind::Steady:
      break;
    case StageLight::Kind::Flicker:
      // Three waves of unrelated rates, which the eye takes for a flame rather than a pulse.
      scale = .82f + .1f * std::sin(t * 13.f) + .06f * std::sin(t * 23.7f + 1.3f) + .05f * std::sin(t * 7.1f + .4f);
      break;
    case StageLight::Kind::Blink: {
      const auto on = std::fmod(t, 1.6f) < .35f;
      scale = on ? 1.f : .0f;
      beaconOn = beaconOn || on;
      break;
    }
    case StageLight::Kind::Stutter: {
      // On, except for a burst of failing every few seconds.
      const auto phase = std::fmod(t, 4.3f);
      if (phase > 3.6f)
        scale = std::sin(phase * 90.f) > .2f ? .15f : 1.f;
      break;
    }
    }
    light->intensity = entry.intensity * std::max(scale, .0f);
  }
  // The glow round a blinking light goes off with it, a burst of one large soft particle each time
  // it comes on.
  if (auto *glow = stage.blinkGlow ? DuelWorld::Get<ParticleEmitter>(app, stage.blinkGlow) : nullptr; glow && beaconOn && !blinkWasOn)
    ++glow->fired;
  blinkWasOn = beaconOn;
}
auto StageSystem::Storm(const float deltaTime) -> void {
  auto *sun = [&]() -> Light * {
    Light *found{};
    DuelWorld::ForEach<Light>(app, [&](const EntityID, Light *light) {
      if (!found && light->type == LightType::Directional)
        found = light;
    });
    return found;
  }();
  if (strike < .0f) {
    nextStrike -= deltaTime;
    if (nextStrike <= .0f) {
      strike = .0f;
      nextStrike = STRIKE_GAP_MIN + (STRIKE_GAP_MAX - STRIKE_GAP_MIN) * Random();
    }
  }
  auto brightness = .0f;
  if (strike >= .0f) {
    for (size_t i = 0; i < std::size(FLICKER_AT); ++i)
      if (strike >= FLICKER_AT[i] && strike < FLICKER_AT[i] + FLICKER_FOR[i])
        brightness = std::max(brightness, FLICKER_STRENGTH[i] * (1.f - (strike - FLICKER_AT[i]) / FLICKER_FOR[i] * .5f));
    strike += deltaTime;
    if (strike > STRIKE_LENGTH)
      strike = -1.f;
  }
  if (sun) {
    sun->intensity = stage.sunIntensity + STRIKE_SUN * brightness;
    sun->diffuse = glm::mix(stage.sunColour, glm::vec3(.85f, .9f, 1.f), brightness);
    sun->specular = sun->diffuse;
  }
  // Over what the fight has put there this frame, never under it.
  if (auto scene = app.GetScene(); scene && flashing && brightness > .0f)
    if (auto *effects = scene->GetAnyComponent<ScreenEffects>()) {
      effects->flash = std::max(effects->flash, STRIKE_FLASH * brightness);
      effects->flashColor = {.85f, .9f, 1.f};
    }
}
