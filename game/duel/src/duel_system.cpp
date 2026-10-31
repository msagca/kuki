#include <GLFW/glfw3.h>
#include <algorithm>
#include <animator.hpp>
#include <application.hpp>
#include <array>
#include <camera.hpp>
#include <cmath>
#include <command_line.hpp>
#include <duel.hpp>
#include <duel_system.hpp>
#include <duel_world.hpp>
#include <entity_manager.hpp>
#include <game_builder.hpp>
#include <glm/common.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/matrix.hpp>
#include <glm/trigonometric.hpp>
#include <graphics_context.hpp>
#include <light.hpp>
#include <light_type.hpp>
#include <model_asset.hpp>
#include <network_system.hpp>
#include <overlay.hpp>
#include <particle_emitter.hpp>
#include <particle_presets.hpp>
#include <scene.hpp>
#include <screen_effects.hpp>
#include <skeleton.hpp>
#include <spdlog/spdlog.h>
#include <transform.hpp>
#include <utility>
#include <vector>
using namespace kuki;
using duel::DuelWorld;
namespace {
/// How far apart the roster stands on the select screen, how far forward the one chosen steps, and
/// how quickly it gets there -- a share of the way each second.
constexpr float LINEUP_SPACING = 1.45f;
constexpr float LINEUP_STEP = 1.1f;
constexpr float LINEUP_EASE = 7.f;
/// How far each figure's head and chest turn to keep the other's head in view, at most, and how
/// quickly that comes and goes -- a share of the way each second.
constexpr float WATCH_LIMIT = 60.f;
constexpr float WATCH_EASE = 8.f;
/// The node of the fighter's skeleton whose travel along the floor is the figure's to take: the
/// clips carry it forwards, and the rules apply that to where the figure stands instead.
constexpr const char *ROOT_NODE = "Root";
/// How the camera follows: how far back it stands with the figures together and how much further
/// for every unit between them, the nearest and furthest it will go, and how quickly it gets there
/// -- a share of the way each second.
constexpr float CAMERA_BASE = 4.4f;
constexpr float CAMERA_SPREAD = .62f;
constexpr float CAMERA_NEAREST = 5.4f;
constexpr float CAMERA_FURTHEST = 9.f;
constexpr float CAMERA_EASE = 4.5f;
/// How long a blow shows on the picture, in seconds, and how hard it shakes the camera at the start.
constexpr float HIT_LENGTH = .55f;
constexpr float HIT_SHAKE = .07f;
/// Blows a bar lasts before it fills again. The rules keep no health and end no match, so the bars
/// are a way of reading the count of blows and nothing the game decides by.
constexpr int BAR_HITS = 10;
/// What a linear colour in the picture's light becomes as an overlay colour, which is written onto
/// the finished picture as it stands.
auto Display(const glm::vec3 &linear, const float alpha) -> glm::vec4 {
  return {glm::pow(linear, glm::vec3(1.f / 2.2f)), alpha};
}
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
  // Presentation only, so read once here and carried into every match. See `StartFight`.
  watching = !HasFlag(GetCommandLineArguments(), NO_IK_FLAG);
  flashing = !HasFlag(GetCommandLineArguments(), NO_FLASH_FLAG);
  spdlog::info("[Duel] Inverse kinematics {}", watching ? "on" : "off");
  if (auto *network = app.GetNetwork(); network) {
    network->SetProtocol(duel::PROTOCOL);
    network->SetBot(duel::ScriptedButtons);
  }
  sparks = DuelWorld::Game(app)
             .Scene("Main")
             .Entity<ParticleObject>("Sparks")
             .With<ParticleEmitter>([](ParticleEmitter &emitter) {
               // Streaks rather than dots, the way a cel draws sparks: each is smeared along the
               // way it flies, and hot enough to bloom.
               emitter = ParticlePresets::Burst();
               emitter.burst = 110;
               emitter.speed = 5.f;
               emitter.speedVariance = .6f;
               emitter.spread = 80.f;
               emitter.lifetime = .45f;
               emitter.drag = 3.f;
               emitter.gravity = {.0f, -4.f, .0f};
               emitter.facing = ParticleFacing::Velocity;
               emitter.stretch = .035f;
               emitter.startSize = .03f;
               emitter.endSize = .01f;
               emitter.startColor = {1.f, .9f, .6f};
               emitter.endColor = {1.f, .35f, .1f};
               emitter.intensity = 9.f;
             })
             .Id();
  shockwave = DuelWorld::Game(app)
                .Scene("Main")
                .Entity<ParticleObject>("Shockwave")
                .With<ParticleEmitter>([](ParticleEmitter &ring) {
                  ring.capacity = 8;
                  ring.emitting = false;
                  ring.burst = 2;
                  ring.radius = .0f;
                  ring.speed = .0f;
                  ring.gravity = {};
                  ring.lifetime = .22f;
                  ring.lifetimeVariance = .3f;
                  ring.sprite = ParticleSprite::Ring;
                  ring.startSize = .2f;
                  ring.endSize = 1.9f;
                  ring.startColor = {1.f, .95f, .85f};
                  ring.endColor = {1.f, .7f, .4f};
                  ring.intensity = 5.f;
                })
                .Id();
  dust = DuelWorld::Game(app)
           .Scene("Main")
           .Entity<ParticleObject>("Dust")
           .With<ParticleEmitter>([](ParticleEmitter &puff) {
             puff.capacity = 256;
             puff.emitting = false;
             puff.burst = 18;
             puff.radius = .15f;
             puff.speed = 1.1f;
             puff.speedVariance = .6f;
             puff.spread = 85.f;
             puff.gravity = {.0f, .3f, .0f};
             puff.drag = 3.f;
             puff.turbulence = .8f;
             puff.lifetime = .9f;
             puff.blend = ParticleBlend::Alpha;
             puff.opacity = .35f;
             puff.startSize = .18f;
             puff.endSize = .7f;
             puff.startColor = {.5f, .48f, .46f};
             puff.endColor = {.4f, .38f, .37f};
             puff.intensity = 1.f;
           })
           .Id();
  // Escape backs out a step at a time: from a match to the select screen, and from there out of the
  // game. Enter on the keyboard or the keypad starts a match.
  escapeAction = app.RegisterInputAction(GLFW_KEY_ESCAPE, [this]() {
    if (screen == Screen::Fight)
      backPending = true;
    else
      app.Quit();
  });
  startAction = app.RegisterInputAction(GLFW_KEY_ENTER, [this]() {
    startPending = screen == Screen::Select;
  });
  startPadAction = app.RegisterInputAction(GLFW_KEY_KP_ENTER, [this]() {
    startPending = screen == Screen::Select;
  });
  spdlog::info("[Duel] A and D choose, Enter fights, Escape goes back; in a match A and D step, J punches, K kicks, D twice lunges");
  EnterSelect();
}
auto DuelSystem::Shutdown() -> void {
  if (auto *network = app.GetNetwork(); network)
    network->End();
  app.UnregisterInputAction(escapeAction);
  app.UnregisterInputAction(startAction);
  app.UnregisterInputAction(startPadAction);
}
auto DuelSystem::Spawn(const int index, const size_t character, const bool alternate) -> EntityID {
  // As a figure and not as a plain model: the seat is what the rest of this finds it by, and it is
  // part of what the entity is made of rather than something added once it is placed.
  const auto entity = DuelWorld::Instantiate<duel::Figure>(app, duel::ModelName(duel::ROSTER[character], alternate));
  if (auto *seat = DuelWorld::Get<duel::Seat>(app, entity); seat)
    seat->player = index;
  auto *animator = DuelWorld::Get<Animator>(app, entity);
  if (!animator || !animator->modelAssetId) {
    spdlog::warn("[Duel] {} has no animator, so they will stand still", duel::ROSTER[character].name);
    return entity;
  }
  // The scene's animator is stepped by the animation system and then made to agree with the rules
  // each tick. It needs the same states to be able to, and the same root, so that the pose does not
  // carry the travel the rules have already taken.
  animator->states = duel::DuelStates();
  animator->rootNode = ROOT_NODE;
  return entity;
}
auto DuelSystem::ClearFigures() -> void {
  std::vector<EntityID> figures;
  DuelWorld::ForEach<duel::Seat>(app, [&](const EntityID id, duel::Seat *) {
    figures.push_back(id);
  });
  for (const auto id : figures)
    app.DeleteEntity(id);
  lineup.clear();
  if (spotlight) {
    app.DeleteEntity(spotlight);
    spotlight = {};
  }
}
auto DuelSystem::EnterSelect() -> void {
  screen = Screen::Select;
  if (auto *network = app.GetNetwork(); network)
    network->End();
  sim.reset();
  ClearFigures();
  hitAge = -1.f;
  cameraPlaced = false;
  for (size_t index = 0; index < duel::ROSTER.size(); ++index) {
    lineup.push_back(Spawn(-1 - static_cast<int>(index), index));
    stepped[index] = index == picks[0] ? 1.f : .0f;
  }
  // From above and in front of the row, aimed at the one chosen; moved along with the choice in
  // `UpdateSelect`.
  spotlight = DuelWorld::Game(app)
                .Scene("Main")
                .Entity<LightObject>("Pick")
                .With<Light>([](Light &light) {
                  light.type = LightType::Spot;
                  light.diffuse = {1.f, .92f, .82f};
                  light.specular = light.diffuse;
                  light.intensity = 22.f;
                  light.constant = 1.f;
                  light.linear = .25f;
                  light.quadratic = .06f;
                  light.innerCutoff = std::cos(glm::radians(16.f));
                  light.outerCutoff = std::cos(glm::radians(26.f));
                })
                .Id();
}
auto DuelSystem::StartFight() -> void {
  const auto count = duel::ROSTER.size();
  // Anyone but the one chosen, from a generator of the screen's own.
  seed ^= seed << 13;
  seed ^= seed >> 17;
  seed ^= seed << 5;
  picks[1] = (picks[0] + 1 + seed % (count - 1)) % count;
  ClearFigures();
  screen = Screen::Fight;
  Spawn(0, picks[0]);
  // In the other colours if both play the same fighter.
  Spawn(1, picks[1], picks[1] == picks[0]);
  // The rules take their clip lengths and the root's travel from the model, so they cannot be made
  // before there is one. Every fighter shares one rig and one set of clips, so whichever the first
  // player chose gives the same rules -- which is what lets two machines whose players chose
  // differently play each other.
  const auto *animator = GetAnimator(0);
  if (const auto *model = animator ? app.GetAsset<ModelAsset>(animator->modelAssetId) : nullptr; model)
    sim = std::make_shared<duel::DuelSim>(*model, app.TickRate(), duel::RootSpace(*model, ROOT_NODE));
  else {
    spdlog::warn("[Duel] The fighter has no model, so the rules will not run");
    sim.reset();
  }
  // Handed to the network system, which plays them from here on: alone, with the other figure
  // played by `Think`, or against another machine, as the command line says. An application without
  // the system has none to hand them to, and Tick plays them itself.
  if (auto *network = app.GetNetwork(); network && sim) {
    network->SetSeat(1, [rules = sim](const int64_t) { return rules->Think(1); });
    network->Begin(sim);
  }
  // `--no-ik` leaves the figures as their clips pose them, so that this and the network can each be
  // tried with and without the other. It is presentation only: the rules never read a goal, so what
  // it changes is what is seen and not what happens, which a match over a network can check by two
  // machines agreeing whether or not they both have it.
  //
  // Each keeps its eyes on the other: the head and the chest share a turn towards the other's
  // head, so that the face follows a duck or a lunge and does not stare at where it was.
  for (auto index = 0; watching && index < duel::DuelSim::PLAYERS; ++index)
    if (auto *scene = GetAnimator(index); scene)
      scene->ik.push_back({.name = "watch", .kind = IKKind::Aim, .bone = "Head", .target = FindBone(1 - index, "Head"), .chain = 2, .limit = WATCH_LIMIT});
  for (auto index = 0; index < duel::DuelSim::PLAYERS; ++index) {
    shownState[index] = sim ? sim->GetAnimator(index).state : 0;
    shownHealth[index] = 1.f;
    trailHealth[index] = 1.f;
  }
  hitAge = -1.f;
  frames = 0;
  cameraPlaced = false;
}
auto DuelSystem::UpdateSelect(const float deltaTime) -> void {
  const auto count = lineup.size();
  const auto ease = 1.f - std::exp(-LINEUP_EASE * deltaTime);
  for (size_t index = 0; index < count; ++index) {
    auto *transform = DuelWorld::Get<Transform>(app, lineup[index]);
    if (!transform)
      continue;
    stepped[index] += ((index == picks[0] ? 1.f : .0f) - stepped[index]) * ease;
    const auto x = (static_cast<float>(index) - static_cast<float>(count - 1) * .5f) * LINEUP_SPACING;
    // Facing the camera, the ones at the ends turned a little in towards the middle, and the one
    // chosen square on.
    transform->position = {x, .0f, -.6f + LINEUP_STEP * stepped[index]};
    transform->rotation = glm::angleAxis(glm::radians(-x * 9.f * (1.f - stepped[index])), glm::vec3(.0f, 1.f, .0f));
    app.MarkTransformDirty(lineup[index]);
  }
  const auto chosenX = (static_cast<float>(picks[0]) - static_cast<float>(count - 1) * .5f) * LINEUP_SPACING;
  if (auto *light = spotlight ? DuelWorld::Get<Light>(app, spotlight) : nullptr) {
    light->position = {chosenX, 4.2f, 2.4f};
    light->forward = glm::normalize(glm::vec3(chosenX, 1.f, -.6f + LINEUP_STEP) - light->position);
  }
  // Nothing on the picture but what the stage lays over it.
  ShowImpact(deltaTime);
  AimCamera({chosenX * .35f, 1.05f, .0f}, 6.4f, deltaTime);
  DrawSelect();
}
auto DuelSystem::DrawSelect() -> void {
  const auto *context = app.GetGraphicsContext();
  auto scene = app.GetScene();
  const auto *camera = scene ? scene->GetCamera() : nullptr;
  if (!context || !camera)
    return;
  const auto [surfaceWidth, surfaceHeight] = context->GetSurfaceSize();
  if (surfaceWidth <= 0 || surfaceHeight <= 0)
    return;
  const auto width = static_cast<float>(surfaceWidth);
  const auto height = static_cast<float>(surfaceHeight);
  const auto unit = height / 1080.f;
  auto &overlay = app.GetOverlay();
  // A `Top` anchor is the middle of the window's top edge, and the offset is measured from there.
  overlay.DrawText("CHOOSE YOUR FIGHTER", {.0f, 54.f * unit}, 54.f * unit, {1.f, 1.f, 1.f, .95f}, TextAnchor::Top);
  // Each name under the feet of the one it names, as the picture has them.
  const auto viewProjection = camera->transform.projection * camera->transform.view;
  for (size_t index = 0; index < lineup.size(); ++index) {
    const auto *transform = DuelWorld::Get<Transform>(app, lineup[index]);
    if (!transform)
      continue;
    const auto clip = viewProjection * glm::vec4(transform->position, 1.f);
    if (clip.w <= .0f)
      continue;
    const auto x = (clip.x / clip.w * .5f + .5f) * width - width * .5f;
    const auto y = (.5f - clip.y / clip.w * .5f) * height + 14.f * unit;
    const auto chosen = index == picks[0];
    overlay.DrawText(duel::ROSTER[index].name, {x, y}, (chosen ? 40.f : 26.f) * unit, {1.f, 1.f, 1.f, chosen ? 1.f : .55f}, TextAnchor::Top);
    if (chosen)
      overlay.DrawText(duel::ROSTER[index].title, {x, y + 46.f * unit}, 22.f * unit, Display({1.f, .62f, .08f}, 1.f), TextAnchor::Top);
  }
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
  // On the select screen the buttons choose: a press of left or right, seen as it goes down so that
  // holding one moves a single place. The rules are not running, so nothing else reads them.
  if (screen == Screen::Select) {
    const auto buttons = app.GetButtons();
    const auto pressed = buttons & ~previousButtons;
    previousButtons = buttons;
    const auto count = duel::ROSTER.size();
    auto moved = false;
    if (pressed & (1u << duel::Left)) {
      picks[0] = (picks[0] + count - 1) % count;
      moved = true;
    }
    if (pressed & (1u << duel::Right)) {
      picks[0] = (picks[0] + 1) % count;
      moved = true;
    }
    // The one chosen throws a punch as it steps forward, which is how a fighter says it is ready.
    if (auto *animator = moved && picks[0] < lineup.size() ? DuelWorld::Get<Animator>(app, lineup[picks[0]]) : nullptr)
      animator->Enter(duel::Punching);
    return;
  }
  previousButtons = app.GetButtons();
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
  ForEachFigure([&](const int player, const EntityID id, Animator &animator, Transform &) {
    const auto *skeleton = player == 0 ? DuelWorld::Get<Skeleton>(app, id) : nullptr;
    const auto *model = skeleton ? app.GetAsset<ModelAsset>(animator.modelAssetId) : nullptr;
    if (!model)
      return;
    for (size_t node = 0; node < model->nodes.size() && node < skeleton->nodeEntities.size(); ++node) {
      if (model->nodes[node].name != ROOT_NODE)
        continue;
      const auto *parent = app.GetEntityComponent<WorldMatrix>(app.GetEntityParent(skeleton->nodeEntities[node]));
      const auto *self = app.GetEntityComponent<WorldMatrix>(id);
      if (!parent || !self)
        return;
      // As `AnimationSystem::Tick` has it: the root's parent in the world, taken back out through the
      // figure's own facing.
      const glm::mat3 facing{glm::normalize(glm::vec3(self->matrix[0])), glm::normalize(glm::vec3(self->matrix[1])), glm::normalize(glm::vec3(self->matrix[2]))};
      const auto live = glm::transpose(facing) * glm::mat3(parent->matrix);
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
  if (startPending) {
    startPending = false;
    if (screen == Screen::Select)
      StartFight();
  }
  if (backPending) {
    backPending = false;
    if (screen == Screen::Fight)
      EnterSelect();
  }
  clock += app.DeltaTime();
  if (screen == Screen::Select) {
    UpdateSelect(app.DeltaTime());
    return;
  }
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
    if (auto *ring = shockwave ? DuelWorld::Get<ParticleEmitter>(app, shockwave) : nullptr; ring) {
      DuelWorld::Game(app).Scene("Main").Entity<ParticleObject>(shockwave).At({impact.x, impact.height, .0f});
      ++ring->fired;
    }
    hitAge = .0f;
    hitStrength = sim->GetAnimator(impact.attacker).state == duel::Kicking ? 1.f : .7f;
    hitPoint = {impact.x, impact.height, .0f};
  }
  // Dust where a figure lunges off or is knocked back: the moment its state becomes one of those.
  for (auto index = 0; index < duel::DuelSim::PLAYERS; ++index) {
    const auto state = sim->GetAnimator(index).state;
    if (state != shownState[index] && (state == duel::Lunge || state == duel::Hit))
      if (auto *puff = dust ? DuelWorld::Get<ParticleEmitter>(app, dust) : nullptr; puff) {
        DuelWorld::Game(app).Scene("Main").Entity<ParticleObject>(dust).At({sim->GetFighter(index).position, .05f, .0f});
        ++puff->fired;
      }
    shownState[index] = state;
  }
  ShowImpact(app.DeltaTime());
  MoveCamera(app.DeltaTime());
  DrawHud();
}
auto DuelSystem::ShowImpact(const float deltaTime) -> void {
  auto scene = app.GetScene();
  auto *effects = scene ? scene->GetAnyComponent<ScreenEffects>() : nullptr;
  if (!effects)
    return;
  // Written whole every frame, so whatever the last blow left is gone once it is over. The stage's
  // system runs after this one and lays its lightning over the top.
  *effects = ScreenEffects{};
  if (hitAge < .0f)
    return;
  const auto t = hitAge;
  const auto s = hitStrength;
  const auto Fade = [t](const float length) {
    return std::clamp(1.f - t / length, .0f, 1.f);
  };
  // The frame a cel animator cuts in as the blow lands: two tones, inverted. A kick holds it for
  // four frames, a punch for two.
  //
  // Both of those and the flash after them are whole-screen changes of brightness several times a
  // minute, which is the thing `--no-flash` is for: without them a blow still has its lines, its
  // smear and its shake.
  const auto impactLength = flashing ? (s > .9f ? .066f : .033f) : .0f;
  effects->impact = t < impactLength ? 1.f : .0f;
  effects->flash = flashing && t >= impactLength ? .4f * s * Fade(.16f) : .0f;
  effects->flashColor = {1.f, .96f, .88f};
  effects->speedLines = s * Fade(.45f);
  effects->radialBlur = .55f * s * Fade(.22f);
  effects->aberration = 9.f * s * Fade(.3f);
  // Centred where the blow landed, as the picture sees it.
  if (const auto *camera = scene->GetCamera()) {
    const auto clip = camera->transform.projection * camera->transform.view * glm::vec4(hitPoint, 1.f);
    if (clip.w > .0f)
      effects->focus = {clip.x / clip.w * .5f + .5f, .5f - clip.y / clip.w * .5f};
  }
  hitAge += deltaTime;
  if (hitAge > HIT_LENGTH)
    hitAge = -1.f;
}
auto DuelSystem::MoveCamera(const float deltaTime) -> void {
  const auto alpha = app.TickAlpha();
  const auto &first = sim->GetFighter(0);
  const auto &second = sim->GetFighter(1);
  const auto a = glm::mix(first.previous, first.position, alpha);
  const auto b = glm::mix(second.previous, second.position, alpha);
  const auto distance = std::clamp(CAMERA_BASE + std::abs(a - b) * CAMERA_SPREAD, CAMERA_NEAREST, CAMERA_FURTHEST);
  AimCamera({(a + b) * .5f, 1.05f, .0f}, distance, deltaTime);
}
auto DuelSystem::AimCamera(const glm::vec3 &focus, const float distance, const float deltaTime) -> void {
  auto scene = app.GetScene();
  auto *camera = scene ? scene->GetCamera() : nullptr;
  if (!camera)
    return;
  // Placed where it belongs on the first frame rather than eased there from wherever the scene was
  // built with it, which would open every match, and the select screen, with a swoop.
  const auto ease = cameraPlaced ? 1.f - std::exp(-CAMERA_EASE * deltaTime) : 1.f;
  cameraPlaced = true;
  cameraFocus += (focus - cameraFocus) * ease;
  cameraDistance += (distance - cameraDistance) * ease;
  // A blow shakes the camera, hard for an instant and settling, along two waves that never line up.
  const auto shake = hitAge >= .0f ? HIT_SHAKE * hitStrength * std::clamp(1.f - hitAge / .3f, .0f, 1.f) : .0f;
  const glm::vec3 jolt{std::sin(clock * 71.f) * shake, std::sin(clock * 53.f + 1.3f) * shake, .0f};
  // Higher as it pulls back, so the floor between two figures far apart still shows.
  camera->position = glm::vec3(cameraFocus.x, 1.3f + (cameraDistance - CAMERA_NEAREST) * .1f, cameraDistance) + jolt;
  camera->rotation = glm::quatLookAt(glm::normalize(cameraFocus + jolt * .5f - camera->position), glm::vec3(.0f, 1.f, .0f));
  ++camera->dirty;
}
auto DuelSystem::DrawHud() -> void {
  const auto *context = app.GetGraphicsContext();
  if (!context)
    return;
  const auto [surfaceWidth, surfaceHeight] = context->GetSurfaceSize();
  if (surfaceWidth <= 0 || surfaceHeight <= 0)
    return;
  const auto width = static_cast<float>(surfaceWidth);
  // Sized against a picture a thousand and eighty high, so the bars keep their place on any window.
  const auto unit = static_cast<float>(surfaceHeight) / 1080.f;
  auto &overlay = app.GetOverlay();
  const auto barWidth = width * .36f;
  const auto barHeight = 22.f * unit;
  const auto top = 36.f * unit;
  const auto gap = 70.f * unit;
  const auto deltaTime = app.DeltaTime();
  for (auto index = 0; index < duel::DuelSim::PLAYERS; ++index) {
    // A player's bar is what the other has landed on them, a bar's worth at a time.
    const auto taken = sim->GetFighter(1 - index).hits;
    const auto wanted = taken > 0 && taken % BAR_HITS == 0 ? .0f : 1.f - static_cast<float>(taken % BAR_HITS) / BAR_HITS;
    shownHealth[index] += (wanted - shownHealth[index]) * std::min(1.f, 18.f * deltaTime);
    // The trail waits and then catches up slowly, which is what lets a blow's damage be read.
    trailHealth[index] = wanted > trailHealth[index] ? wanted : trailHealth[index] + (shownHealth[index] - trailHealth[index]) * std::min(1.f, 1.8f * deltaTime);
    const auto right = index == 1;
    const auto edge = right ? width * .5f + gap * .5f : width * .5f - gap * .5f;
    // An anchor is a corner of the window and the offset is measured from it, so what is held by its
    // right edge is placed by how far that edge is from the window's.
    const auto frameLeft = right ? edge - 3.f * unit : edge - barWidth - 3.f * unit;
    overlay.DrawRect({frameLeft, top - 3.f * unit}, {barWidth + 6.f * unit, barHeight + 6.f * unit}, {.02f, .02f, .04f, .85f}, TextAnchor::TopLeft);
    // Drawn from the middle outwards, as a fighting game's bars are, so they drain towards the edges.
    const auto Fill = [&](const float share, const glm::vec4 &colour) {
      const auto length = barWidth * std::clamp(share, .0f, 1.f);
      const auto start = right ? edge + barWidth - length : edge - barWidth;
      overlay.DrawRect({start, top}, {length, barHeight}, colour, TextAnchor::TopLeft);
    };
    // What is left is held against the outer edge, so a blow takes its bite out of the end nearest
    // the middle, where both players are looking.
    Fill(trailHealth[index], {1.f, .92f, .8f, .9f});
    const auto health = shownHealth[index];
    Fill(health, health > .3f ? Display({1.f, .62f, .08f}, 1.f) : Display({1.f, .12f, .05f}, 1.f));
    overlay.DrawText(duel::ROSTER[picks[index]].name, {right ? edge + barWidth - width : edge - barWidth, top + barHeight + 8.f * unit}, 26.f * unit, {1.f, 1.f, 1.f, .95f}, right ? TextAnchor::TopRight : TextAnchor::TopLeft);
  }
}
