#include <GLFW/glfw3.h>
#include <algorithm>
#include <animator.hpp>
#include <application.hpp>
#include <cmath>
#include <duel.hpp>
#include <entity_manager.hpp>
#include <game_builder.hpp>
#include <glm/common.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/trigonometric.hpp>
#include <initializer_list>
#include <match.hpp>
#include <overlay.hpp>
#include <particle_emitter.hpp>
#include <particle_presets.hpp>
#include <script_registry.hpp>
#include <spdlog/spdlog.h>
#include <transform.hpp>
#include <utility>
#include <vector>
using namespace kuki;
namespace {
/// How close two figures may stand, and how far either may go from the middle of the floor.
constexpr float MIN_GAP = .75f;
constexpr float FLOOR_HALF = 6.f;
/// How far in front of itself a figure reaches with each, and how high off the floor it lands.
constexpr float PUNCH_REACH = 1.3f;
constexpr float PUNCH_HEIGHT = 1.45f;
constexpr float KICK_REACH = 1.5f;
constexpr float KICK_HEIGHT = 1.05f;
/// The tick of each clip on which the limb is out. The clips are keyed at thirty frames a second
/// and the game ticks at sixty, so these are twice the frame the pose is keyed on.
/// Where the column of key hints starts, how big its text is and how far apart its lines are.
constexpr float HINT_LEFT = 24.f;
constexpr float HINT_TOP = 64.f;
constexpr float HINT_SIZE = 18.f;
constexpr float HINT_SPACING = 26.f;
/// How far right of the keys the column saying what they do begins.
constexpr float HINT_ACTION = 56.f;
constexpr int PUNCH_TICK = 18;
constexpr int KICK_TICK = 24;
auto MakeStates() -> std::vector<AnimationState> {
  return {
    {.name = "idle", .clip = "Idle", .loop = true},
    {.name = "walk", .clip = "Walk", .loop = true},
    {.name = "back", .clip = "Walk", .loop = true, .speed = .7f},
    {.name = "lunge", .clip = "Walk", .speed = 2.2f, .fade = 3, .next = "idle"},
    {.name = "punch", .clip = "Punch", .fade = 3, .next = "idle", .events = {{PUNCH_TICK, "hit"}}},
    {.name = "kick", .clip = "Kick", .fade = 3, .next = "idle", .events = {{KICK_TICK, "hit"}}},
    {.name = "hit", .clip = "Hit", .fade = 2, .next = "idle"},
  };
}
} // namespace
Match::Match()
  : Script(std::in_place_type<Match>) {}
auto Match::CloneTo(EntityManager &entityManager, const EntityID id) const -> void {
  entityManager.AddComponent<Match>(id);
}
auto Match::GetTypeName() const -> std::string {
  return "Match";
}
auto Match::Start(Application &app) -> void {
  // The order is the order of the bits, and the names below it are the same buttons in the same
  // order under the short names the gestures are written with.
  app.RegisterButton("Duel.Left", InputManager::Trigger{GLFW_KEY_A, 0}, "Step left");
  app.RegisterButton("Duel.Right", InputManager::Trigger{GLFW_KEY_D, 0}, "Step right");
  app.RegisterButton("Duel.Punch", InputManager::Trigger{GLFW_KEY_J, 0}, "Punch");
  app.RegisterButton("Duel.Kick", InputManager::Trigger{GLFW_KEY_K, 0}, "Kick");
  const std::vector<std::string> names{"left", "right", "punch", "kick"};
  punch = gestures.Add("punch", "punch", names);
  kick = gestures.Add("kick", "kick", names);
  lunge = gestures.Add("lunge", "right right", names, 12);
  Spawn(app, 0, -1.6f, 1.f);
  Spawn(app, 1, 1.6f, -1.f);
  sparks = app.Game()
             .Scene("Main")
             .Entity("Sparks")
             .With<ParticleEmitter>([](ParticleEmitter &emitter) {
               emitter = ParticlePresets::Burst();
               emitter.burst = 90;
               emitter.speed = 3.f;
               emitter.startSize = .045f;
             })
             .Id();
  quitAction = app.RegisterInputAction(GLFW_KEY_ESCAPE, [this]() {
    if (auto *self = GetApp(); self)
      self->Quit();
  });
  spdlog::info("[Duel] A and D step, J punches, K kicks, D twice lunges, Escape quits");
}
auto Match::Shutdown(Application &app) -> void {
  app.UnregisterInputAction(quitAction);
}
auto Match::Spawn(Application &app, const int index, const float position, const float facing) -> void {
  auto &fighter = fighters[index];
  fighter = {.entity = app.InstantiateAsset(std::string(FIGHTER_MODEL)), .position = position, .previous = position, .facing = facing};
  auto *animator = GetAnimator(app, index);
  if (!animator) {
    spdlog::warn("[Duel] Fighter {} has no animator, so it will stand still", index);
    return;
  }
  animator->states = MakeStates();
  animator->rootNode = "Root";
}
auto Match::GetAnimator(Application &app, const int index) -> Animator * {
  return fighters[index].entity ? app.GetEntityComponent<Animator>(fighters[index].entity) : nullptr;
}
auto Match::IsFree(const Animator &animator) const -> bool {
  const auto state = animator.GetStateName();
  return state == "idle" || state == "walk" || state == "back";
}
auto Match::Tick(Application &app) -> void {
  const auto buttons = app.GetButtons();
  auto wantsPunch = false, wantsKick = false, wantsLunge = false;
  for (const auto id : gestures.Advance(buttons)) {
    wantsPunch |= id == punch;
    wantsKick |= id == kick;
    wantsLunge |= id == lunge;
  }
  // What the animations did this tick first, since that has already happened: travel, and any blow
  // that landed. Then what each figure starts next, which the animator takes up on the tick after.
  Step(app, 0);
  Step(app, 1);
  auto &left = fighters[0];
  auto &right = fighters[1];
  if (right.position - left.position < MIN_GAP) {
    const auto overlap = MIN_GAP - (right.position - left.position);
    left.position -= overlap * .5f;
    right.position += overlap * .5f;
  }
  for (auto &fighter : fighters)
    fighter.position = std::clamp(fighter.position, -FLOOR_HALF, FLOOR_HALF);
  if (auto *animator = GetAnimator(app, 0); animator && IsFree(*animator)) {
    const auto held = [buttons](const Button button) { return (buttons >> button & 1u) != 0; };
    const auto direction = static_cast<int>(held(Right)) - static_cast<int>(held(Left));
    if (wantsLunge)
      animator->Play("lunge");
    else if (wantsKick)
      animator->Play("kick");
    else if (wantsPunch)
      animator->Play("punch");
    else
      animator->Play(direction > 0 ? "walk" : direction < 0 ? "back" : "idle");
  }
  // The other figure is not played by anybody. It closes the distance, and now and then, on a
  // tick that depends on nothing but the count of them, throws a punch.
  if (auto *animator = GetAnimator(app, 1); animator && IsFree(*animator)) {
    const auto distance = right.position - left.position;
    if (distance > 2.4f)
      animator->Play("walk");
    else if (distance < PUNCH_REACH && app.TickIndex() % 150 == 0)
      animator->Play("punch");
    else
      animator->Play("idle");
  }
}
auto Match::Step(Application &app, const int index) -> void {
  auto &fighter = fighters[index];
  fighter.previous = fighter.position;
  const auto *animator = GetAnimator(app, index);
  if (!animator)
    return;
  const auto state = animator->GetStateName();
  // The walk is the only clip there is for stepping, and it only goes forwards. Stepping back is
  // the same stride with the travel turned round.
  fighter.position += fighter.facing * animator->rootDelta.z * (state == "back" ? -1.f : 1.f);
  if (!animator->Fired("hit"))
    return;
  if (state == "kick")
    Strike(app, index, KICK_REACH, KICK_HEIGHT);
  else
    Strike(app, index, PUNCH_REACH, PUNCH_HEIGHT);
}
auto Match::Strike(Application &app, const int index, const float reach, const float height) -> void {
  auto &fighter = fighters[index];
  const auto &other = fighters[1 - index];
  const auto distance = std::abs(other.position - fighter.position);
  spdlog::info("[Duel] tick {}: fighter {} strikes at {:.2f} with reach {:.2f}: {}", app.TickIndex(), index, distance, reach, distance <= reach ? "hit" : "miss");
  if (distance > reach)
    return;
  ++fighter.hits;
  if (auto *animator = GetAnimator(app, 1 - index); animator)
    animator->Play("hit", true);
  if (auto *emitter = sparks ? app.GetEntityComponent<ParticleEmitter>(sparks) : nullptr; emitter) {
    app.Game().Scene("Main").Entity(sparks).At({other.position - fighter.facing * .2f, height, .0f});
    ++emitter->fired;
  }
}
auto Match::Update(Application &app) -> void {
  const auto alpha = app.TickAlpha();
  for (const auto &fighter : fighters) {
    auto *transform = fighter.entity ? app.GetEntityComponent<Transform>(fighter.entity) : nullptr;
    if (!transform)
      continue;
    // The model looks along +Z. A quarter turn one way or the other lays that along the floor.
    transform->position = {glm::mix(fighter.previous, fighter.position, alpha), .0f, .0f};
    transform->rotation = glm::angleAxis(glm::radians(90.f * fighter.facing), glm::vec3(.0f, 1.f, .0f));
    app.MarkTransformDirty(fighter.entity);
  }
  // The keys, one to a line down the left, and nothing else. Below the top by more than a line so
  // that the column clears the bar the editor lays over that corner when the game is played in it.
  auto &overlay = app.GetOverlay();
  auto line = 0;
  // The key and what it does are drawn apart, each at its own left edge. Written as one string they
  // would not line up: the font's letters are not all one width, so no run of spaces is a column.
  for (const auto &[key, action] : std::initializer_list<std::pair<const char *, const char *>>{{"A  D", "step"}, {"J", "punch"}, {"K", "kick"}, {"D  D", "lunge"}, {"Esc", "quit"}}) {
    const auto y = HINT_TOP + HINT_SPACING * static_cast<float>(line++);
    overlay.DrawText(key, {HINT_LEFT, y}, HINT_SIZE, {1.f, 1.f, 1.f, .9f});
    overlay.DrawText(action, {HINT_LEFT + HINT_ACTION, y}, HINT_SIZE, {1.f, 1.f, 1.f, .65f});
  }
}
KUKI_REGISTER_SCRIPT(Match)
