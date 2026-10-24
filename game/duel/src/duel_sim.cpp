#include <algorithm>
#include <animation_system.hpp>
#include <animator_state.hpp>
#include <cmath>
#include <duel_sim.hpp>
#include <glm/geometric.hpp>
#include <model_asset.hpp>
#include <spdlog/spdlog.h>
using namespace kuki;
namespace duel {
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
constexpr int PUNCH_TICK = 18;
constexpr int KICK_TICK = 24;
/// How far apart the unattended player wants to stand, and how often it throws a punch, in ticks.
constexpr float CLOSE_ENOUGH = 2.4f;
constexpr int PUNCH_EVERY = 150;
/// How many ticks of impacts are kept for whatever draws them to find. Far longer than a rollback
/// reaches, so that a blow thrown again is always found.
constexpr int64_t IMPACT_MEMORY = 300;
/// The buttons as the figure sees them: forwards is `Right` whichever way along the floor that is.
/// The keyboard is the same for both players, but a lunge is two taps forwards, and for the one
/// facing down the floor that is two taps of the other key.
auto Relative(const ButtonMask mask, const float facing) -> ButtonMask {
  if (facing >= 0.f)
    return mask;
  const auto left = mask >> Left & 1u;
  const auto right = mask >> Right & 1u;
  return (mask & ~(1u << Left | 1u << Right)) | left << Right | right << Left;
}
auto Go(Animator &animator, const int state, const bool restart = false) -> void {
  if (restart || animator.state != state)
    animator.Enter(state);
}
} // namespace
auto ScriptedButtons(const int player, const int64_t tick) -> ButtonMask {
  auto hash = static_cast<uint32_t>(tick / (6 + player)) * 2654435761u + static_cast<uint32_t>(player) * 40503u;
  hash ^= hash >> 15;
  ButtonMask mask = 0;
  // Forwards seven ticks in eight, whichever key that is for this figure.
  if ((hash & 7u) != 0)
    mask |= 1u << (player == 0 ? Right : Left);
  auto blow = static_cast<uint32_t>(tick) * 2246822519u + static_cast<uint32_t>(player) * 3266489917u;
  blow ^= blow >> 13;
  if (blow % 37u == 0)
    mask |= 1u << Punch;
  else if (blow % 41u == 1)
    mask |= 1u << Kick;
  return mask;
}
auto DuelStates() -> std::vector<AnimationState> {
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
auto RootSpace(const ModelAsset &model, const std::string_view rootNode) -> glm::mat3 {
  auto root = -1;
  for (size_t index = 0; index < model.nodes.size(); ++index)
    if (model.nodes[index].name == rootNode) {
      root = static_cast<int>(index);
      break;
    }
  if (root < 0)
    return glm::mat3(1.f);
  // From the node the root hangs from up to, but not including, the top one, which is the entity
  // itself: its place and turn are the game's to set, so only its size is kept.
  glm::mat4 between(1.f);
  auto node = model.nodes[root].parent;
  while (node >= 0 && model.nodes[node].parent >= 0) {
    between = model.nodes[node].transform * between;
    node = model.nodes[node].parent;
  }
  glm::mat3 scale(1.f);
  if (node >= 0) {
    const glm::mat3 top(model.nodes[node].transform);
    scale[0][0] = glm::length(top[0]);
    scale[1][1] = glm::length(top[1]);
    scale[2][2] = glm::length(top[2]);
  }
  return scale * glm::mat3(between);
}
DuelSim::DuelSim(const ModelAsset &model, const int tickRate, const glm::mat3 &rootSpace)
  : model(model), tickRate(tickRate), rootSpace(rootSpace) {
  const std::vector<std::string> names{"left", "right", "punch", "kick"};
  for (auto player = 0; player < PLAYERS; ++player) {
    auto &recognizer = gestures[player];
    punch = recognizer.Add("punch", "punch", names);
    kick = recognizer.Add("kick", "kick", names);
    lunge = recognizer.Add("lunge", "right right", names, 12);
    animators[player].states = DuelStates();
    animators[player].rootNode = "Root";
    animators[player].Enter(Idle);
  }
  fighters[0] = {.position = -1.6f, .previous = -1.6f, .facing = 1.f};
  fighters[1] = {.position = 1.6f, .previous = 1.6f, .facing = -1.f};
}
auto DuelSim::GetTick() const -> int64_t {
  return tick;
}
auto DuelSim::GetRootSpace() const -> const glm::mat3 & {
  return rootSpace;
}
auto DuelSim::GetFighter(const int player) const -> const Fighter & {
  return fighters[player];
}
auto DuelSim::GetAnimator(const int player) const -> const Animator & {
  return animators[player];
}
auto DuelSim::GetImpacts() -> std::vector<Impact> & {
  return impacts;
}
auto DuelSim::SetLogging(const bool value) -> void {
  logging = value;
}
auto DuelSim::IsFree(const Animator &animator) const -> bool {
  return animator.state == Idle || animator.state == Walk || animator.state == Back;
}
auto DuelSim::Think(const int player) const -> ButtonMask {
  const auto &self = fighters[player];
  const auto distance = std::abs(fighters[1 - player].position - self.position);
  if (distance > CLOSE_ENOUGH)
    return 1u << (self.facing > 0.f ? Right : Left);
  if (distance < PUNCH_REACH && tick % PUNCH_EVERY == 0)
    return 1u << Punch;
  return 0;
}
auto DuelSim::Advance(const int64_t, const std::span<const ButtonMask> buttons, const bool replay) -> void {
  replaying = replay;
  // What the animations did this tick first, since that has already happened: travel, and any blow
  // that landed. Then what each figure starts next, which the animator takes up on the tick after.
  for (auto &animator : animators) {
    StepAnimator(animator, model, tickRate);
    animator.rootDelta = rootSpace * animator.rootDelta;
  }
  std::array<ButtonMask, PLAYERS> held{};
  std::array<bool, PLAYERS> wantsPunch{}, wantsKick{}, wantsLunge{};
  for (auto player = 0; player < PLAYERS; ++player) {
    held[player] = Relative(static_cast<size_t>(player) < buttons.size() ? buttons[player] : 0u, fighters[player].facing);
    for (const auto id : gestures[player].Advance(held[player])) {
      wantsPunch[player] |= id == punch;
      wantsKick[player] |= id == kick;
      wantsLunge[player] |= id == lunge;
    }
  }
  Step(0);
  Step(1);
  auto &left = fighters[0];
  auto &right = fighters[1];
  if (right.position - left.position < MIN_GAP) {
    const auto overlap = MIN_GAP - (right.position - left.position);
    left.position -= overlap * .5f;
    right.position += overlap * .5f;
  }
  for (auto &fighter : fighters)
    fighter.position = std::clamp(fighter.position, -FLOOR_HALF, FLOOR_HALF);
  for (auto player = 0; player < PLAYERS; ++player) {
    auto &animator = animators[player];
    if (!IsFree(animator))
      continue;
    const auto direction = static_cast<int>(held[player] >> Right & 1u) - static_cast<int>(held[player] >> Left & 1u);
    if (wantsLunge[player])
      Go(animator, Lunge);
    else if (wantsKick[player])
      Go(animator, Kicking);
    else if (wantsPunch[player])
      Go(animator, Punching);
    else
      Go(animator, direction > 0 ? Walk : direction < 0 ? Back : Idle);
  }
  ++tick;
}
auto DuelSim::Step(const int player) -> void {
  auto &fighter = fighters[player];
  fighter.previous = fighter.position;
  const auto &animator = animators[player];
  // The walk is the only clip there is for stepping, and it only goes forwards. Stepping back is
  // the same stride with the travel turned round.
  fighter.position += fighter.facing * animator.rootDelta.z * (animator.state == Back ? -1.f : 1.f);
  if (!animator.Fired("hit"))
    return;
  if (animator.state == Kicking)
    Strike(player, KICK_REACH, KICK_HEIGHT);
  else
    Strike(player, PUNCH_REACH, PUNCH_HEIGHT);
}
auto DuelSim::Strike(const int player, const float reach, const float height) -> void {
  auto &fighter = fighters[player];
  const auto &other = fighters[1 - player];
  const auto distance = std::abs(other.position - fighter.position);
  const auto landed = distance <= reach;
  if (logging && !replaying)
    spdlog::info("[Duel] tick {}: fighter {} strikes at {:.2f} with reach {:.2f}: {}", tick, player, distance, reach, landed ? "hit" : "miss");
  const auto where = other.position - fighter.facing * .2f;
  // A tick run again finds its own record and writes over it, keeping whether it has been shown.
  auto found = std::ranges::find_if(impacts, [&](const Impact &impact) { return impact.tick == tick && impact.attacker == player; });
  if (found == impacts.end()) {
    impacts.push_back({.tick = tick, .attacker = player});
    found = std::prev(impacts.end());
  }
  found->hit = landed;
  found->x = where;
  found->height = height;
  std::erase_if(impacts, [this](const Impact &impact) { return impact.tick < tick - IMPACT_MEMORY; });
  if (!landed)
    return;
  ++fighter.hits;
  Go(animators[1 - player], Hit, true);
}
auto DuelSim::Save(std::vector<std::byte> &out) const -> void {
  StateWriter writer(out);
  writer.Write(tick);
  for (auto player = 0; player < PLAYERS; ++player) {
    const auto &fighter = fighters[player];
    writer.Write(fighter.position);
    writer.Write(fighter.previous);
    writer.Write(fighter.facing);
    writer.Write(static_cast<int32_t>(fighter.hits));
    SaveAnimatorTick(writer, animators[player]);
    const auto memory = gestures[player].GetMemory();
    writer.Write(memory.history);
    writer.Write(memory.count);
  }
}
auto DuelSim::Load(const std::span<const std::byte> in) -> void {
  StateReader reader(in);
  tick = reader.Read<int64_t>();
  for (auto player = 0; player < PLAYERS; ++player) {
    auto &fighter = fighters[player];
    fighter.position = reader.Read<float>();
    fighter.previous = reader.Read<float>();
    fighter.facing = reader.Read<float>();
    fighter.hits = reader.Read<int32_t>();
    LoadAnimatorTick(reader, animators[player]);
    GestureRecognizer::Memory memory;
    memory.history = reader.Read<std::array<ButtonMask, GestureRecognizer::HISTORY>>();
    memory.count = reader.Read<int64_t>();
    gestures[player].SetMemory(memory);
  }
  if (!reader.Ok())
    spdlog::error("[Duel] A snapshot ran out before the state did");
}
} // namespace duel
