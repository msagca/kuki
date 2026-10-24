#include <GLFW/glfw3.h>
#include <algorithm>
#include <animator.hpp>
#include <application.hpp>
#include <array>
#include <cmath>
#include <command_line.hpp>
#include <duel.hpp>
#include <duel_system.hpp>
#include <duel_world.hpp>
#include <entity_manager.hpp>
#include <game_builder.hpp>
#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/matrix.hpp>
#include <glm/trigonometric.hpp>
#include <initializer_list>
#include <model_asset.hpp>
#include <network_system.hpp>
#include <overlay.hpp>
#include <particle_emitter.hpp>
#include <particle_presets.hpp>
#include <skeleton.hpp>
#include <spdlog/spdlog.h>
#include <transform.hpp>
#include <utility>
#include <vector>
using namespace kuki;
using duel::DuelWorld;
namespace {
/// Where the column of key hints starts, how big its text is and how far apart its lines are.
constexpr float HINT_LEFT = 24.f;
constexpr float HINT_TOP = 64.f;
constexpr float HINT_SIZE = 18.f;
constexpr float HINT_SPACING = 26.f;
/// How far right of the keys the column saying what they do begins.
constexpr float HINT_ACTION = 56.f;
/// How far each figure's head and chest turn to keep the other's head in view, at most, and how
/// quickly that comes and goes -- a share of the way each second.
constexpr float WATCH_LIMIT = 60.f;
constexpr float WATCH_EASE = 8.f;
/// The node of the fighter's skeleton whose travel along the floor is the figure's to take: the
/// clips carry it forwards, and the rules apply that to where the figure stands instead.
constexpr const char *ROOT_NODE = "Root";
} // namespace
DuelSystem::DuelSystem(Application &app)
  : System(std::in_place_type<DuelSystem>, app) {}
auto DuelSystem::PostStart() -> void {
  // The order is the order of the bits, and the names below it are the same buttons in the same
  // order under the short names the gestures are written with.
  app.RegisterButton("Duel.Left", InputManager::Trigger{GLFW_KEY_A, 0}, "Step left");
  app.RegisterButton("Duel.Right", InputManager::Trigger{GLFW_KEY_D, 0}, "Step right");
  app.RegisterButton("Duel.Punch", InputManager::Trigger{GLFW_KEY_J, 0}, "Punch");
  app.RegisterButton("Duel.Kick", InputManager::Trigger{GLFW_KEY_K, 0}, "Kick");
  for (auto index = 0; index < duel::DuelSim::PLAYERS; ++index)
    Spawn(index);
  // The rules take their clip lengths and the root's travel from the model, so they cannot be made
  // before there is one. Without a fighter there is nobody to play and nothing to run.
  const auto *animator = GetAnimator(0);
  if (const auto *model = animator ? app.GetAsset<ModelAsset>(animator->modelAssetId) : nullptr; model)
    sim = std::make_shared<duel::DuelSim>(*model, app.TickRate(), duel::RootSpace(*model, ROOT_NODE));
  else
    spdlog::warn("[Duel] The fighter has no model, so the rules will not run");
  // Handed to the network system, which plays them from here on: alone, with the other figure
  // played by `Think`, or against another machine, as the command line says. An application without
  // the system has none to hand them to, and Tick plays them itself.
  if (auto *network = app.GetNetwork(); network && sim) {
    network->SetProtocol(duel::PROTOCOL);
    network->SetSeat(1, [rules = sim](const int64_t) { return rules->Think(1); });
    network->SetBot(duel::ScriptedButtons);
    network->Begin(sim);
  }
  // `--no-ik` leaves the figures as their clips pose them, so that this and the network can each be
  // tried with and without the other. It is presentation only: the rules never read a goal, so what
  // it changes is what is seen and not what happens, which a match over a network can check by two
  // machines agreeing whether or not they both have it.
  const auto watching = !HasFlag(GetCommandLineArguments(), NO_IK_FLAG);
  spdlog::info("[Duel] Inverse kinematics {}", watching ? "on" : "off");
  // Each keeps its eyes on the other: the head and the chest share a turn towards the other's
  // head, so that the face follows a duck or a lunge and does not stare at where it was.
  for (auto index = 0; watching && index < duel::DuelSim::PLAYERS; ++index)
    if (auto *scene = GetAnimator(index); scene)
      scene->ik.push_back({.name = "watch", .kind = IKKind::Aim, .bone = "Head", .target = FindBone(1 - index, "Head"), .chain = 2, .limit = WATCH_LIMIT});
  sparks = DuelWorld::Game(app)
             .Scene("Main")
             .Entity<ParticleObject>("Sparks")
             .With<ParticleEmitter>([](ParticleEmitter &emitter) {
               emitter = ParticlePresets::Burst();
               emitter.burst = 90;
               emitter.speed = 3.f;
               emitter.startSize = .045f;
             })
             .Id();
  quitAction = app.RegisterInputAction(GLFW_KEY_ESCAPE, [this]() {
    app.Quit();
  });
  spdlog::info("[Duel] A and D step, J punches, K kicks, D twice lunges, Escape quits");
}
auto DuelSystem::Shutdown() -> void {
  if (auto *network = app.GetNetwork(); network)
    network->End();
  app.UnregisterInputAction(quitAction);
}
auto DuelSystem::Spawn(const int index) -> EntityID {
  // As a figure and not as a plain model: the seat is what the rest of this finds it by, and it is
  // part of what the entity is made of rather than something added once it is placed.
  const auto entity = DuelWorld::Instantiate<duel::Figure>(app, FIGHTER_MODEL);
  if (auto *seat = DuelWorld::Get<duel::Seat>(app, entity); seat)
    seat->player = index;
  auto *animator = DuelWorld::Get<Animator>(app, entity);
  if (!animator || !animator->modelAssetId) {
    spdlog::warn("[Duel] Fighter {} has no animator, so it will stand still", index);
    return entity;
  }
  // The scene's animator is stepped by the animation system and then made to agree with the rules
  // each tick. It needs the same states to be able to, and the same root, so that the pose does not
  // carry the travel the rules have already taken.
  animator->states = duel::DuelStates();
  animator->rootNode = ROOT_NODE;
  return entity;
}
auto DuelSystem::ForEachFigure(auto &&func) -> void {
  DuelWorld::ForEach<duel::Seat, Animator, Transform>(app, [&](const EntityID id, duel::Seat *seat, Animator *animator, Transform *transform) {
    if (seat->player >= 0 && seat->player < duel::DuelSim::PLAYERS)
      func(seat->player, id, *animator, *transform);
  });
}
auto DuelSystem::GetAnimator(const int player) -> Animator * {
  Animator *found = nullptr;
  ForEachFigure([&](const int seat, const EntityID, Animator &animator, Transform &) {
    if (seat == player)
      found = &animator;
  });
  return found;
}
auto DuelSystem::FindBone(const int player, const std::string &name) -> EntityID {
  auto bone = EntityID::Invalid;
  ForEachFigure([&](const int seat, const EntityID id, Animator &animator, Transform &) {
    const auto *skeleton = seat == player ? DuelWorld::Get<Skeleton>(app, id) : nullptr;
    const auto *model = skeleton ? app.GetAsset<ModelAsset>(animator.modelAssetId) : nullptr;
    if (!model)
      return;
    for (size_t node = 0; node < model->nodes.size() && node < skeleton->nodeEntities.size(); ++node)
      if (model->nodes[node].name == name)
        bone = skeleton->nodeEntities[node];
  });
  return bone;
}
auto DuelSystem::Tick() -> void {
  if (!sim)
    return;
  // With a network system the rules have already been played by the time this runs, however many
  // times that took -- a tick, none while waiting for the other player, or several to put right a
  // guess -- and the scene only has to be made to agree with wherever they have got to. Without
  // one, this plays the tick itself: the player is the first figure and the second is played by
  // nobody, from the keyboard's buttons as they stood for this tick and nothing else.
  if (!app.GetNetwork()) {
    const std::array<ButtonMask, duel::DuelSim::PLAYERS> buttons{app.GetButtons(), sim->Think(1)};
    sim->Advance(sim->GetTick(), buttons, false);
  }
  SyncAnimators();
}
auto DuelSystem::SyncAnimators() -> void {
  ForEachFigure([&](const int player, const EntityID, Animator &scene, Transform &) {
    const auto &rules = sim->GetAnimator(player);
    // Entering is what starts the fade out of the pose that was showing, so it is done when the
    // state changes, and when the same one begins again -- which is the clock going back.
    if (scene.state != rules.state || rules.stateTicks < scene.stateTicks)
      scene.Enter(rules.state);
    scene.stateTicks = rules.stateTicks;
    scene.finished = rules.finished;
    scene.phase = rules.phase;
    scene.previousPhase = rules.previousPhase;
  });
}
auto DuelSystem::CheckRootSpace() -> void {
  ForEachFigure([&](const int player, const EntityID id, Animator &animator, Transform &self) {
    const auto *skeleton = player == 0 ? DuelWorld::Get<Skeleton>(app, id) : nullptr;
    const auto *model = skeleton ? app.GetAsset<ModelAsset>(animator.modelAssetId) : nullptr;
    if (!model)
      return;
    for (size_t node = 0; node < model->nodes.size() && node < skeleton->nodeEntities.size(); ++node) {
      if (model->nodes[node].name != ROOT_NODE)
        continue;
      const auto *parent = app.GetEntityComponent<Transform>(app.GetEntityParent(skeleton->nodeEntities[node]));
      if (!parent)
        return;
      // As `AnimationSystem::Tick` has it: the root's parent in the world, taken back out through the
      // figure's own facing.
      const glm::mat3 facing{glm::normalize(glm::vec3(self.world[0])), glm::normalize(glm::vec3(self.world[1])), glm::normalize(glm::vec3(self.world[2]))};
      const auto live = glm::transpose(facing) * glm::mat3(parent->world);
      const auto &mine = sim->GetRootSpace();
      auto difference = .0f;
      for (auto column = 0; column < 3; ++column)
        for (auto row = 0; row < 3; ++row)
          difference = std::max(difference, std::abs(live[column][row] - mine[column][row]));
      spdlog::debug("[Duel] Root space differs from the scene's by at most {:.5f}", difference);
      if (difference > 1e-3f)
        spdlog::warn("[Duel] The rules and the animation system disagree about how far the root travels, so the figures will not walk at the speed the clips were made for");
      return;
    }
  });
}
auto DuelSystem::Update(const float) -> void {
  if (!sim)
    return;
  if (++frames == 10)
    CheckRootSpace();
  const auto alpha = app.TickAlpha();
  ForEachFigure([&](const int player, const EntityID id, Animator &animator, Transform &transform) {
    const auto &fighter = sim->GetFighter(player);
    // The model looks along +Z. A quarter turn one way or the other lays that along the floor.
    transform.position = {glm::mix(fighter.previous, fighter.position, alpha), .0f, .0f};
    transform.rotation = glm::angleAxis(glm::radians(90.f * fighter.facing), glm::vec3(.0f, 1.f, .0f));
    app.MarkTransformDirty(id);
    // A blow knocks the head back, and a figure that kept looking straight on through it would not
    // seem to have felt it. So the watching lets go while one lands and comes back after.
    if (auto *watch = animator.FindGoal("watch"); watch) {
      const auto wanted = animator.state == duel::Hit ? .0f : 1.f;
      watch->weight += (wanted - watch->weight) * std::min(1.f, WATCH_EASE * app.DeltaTime());
    }
  });
  // The sparks are drawn from what the rules have recorded and not set off by them. A blow that a
  // later correction throws again, or throws somewhere else, is found by its tick and not shown a
  // second time; one that turns out never to have landed has already been seen, and that is the
  // price of showing it when it first seemed to.
  for (auto &impact : sim->GetImpacts()) {
    if (impact.shown)
      continue;
    impact.shown = true;
    if (!impact.hit)
      continue;
    if (auto *emitter = sparks ? DuelWorld::Get<ParticleEmitter>(app, sparks) : nullptr; emitter) {
      DuelWorld::Game(app).Scene("Main").Entity<ParticleObject>(sparks).At({impact.x, impact.height, .0f});
      ++emitter->fired;
    }
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
