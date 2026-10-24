#include <algorithm>
#include <array>
#include <duel_sim.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/trigonometric.hpp>
#include <gtest/gtest.h>
#include <map>
#include <model_asset.hpp>
#include <rollback.hpp>
#include <simulated_link.hpp>
using namespace kuki;
using namespace duel;
namespace {
constexpr int RATE = 60;
auto Clip(std::string name, const float seconds) -> AnimationClip {
  AnimationClip clip;
  clip.name = std::move(name);
  clip.duration = seconds * 30.f;
  clip.ticksPerSecond = 30.f;
  return clip;
}
/// @brief A fighter with the five clips the game asks for, and a walk that carries the root two
/// units along Z in a second, which is a thirtieth of a unit a tick.
auto MakeModel() -> ModelAsset {
  ModelAsset model{AssetID{}};
  model.nodes.resize(2);
  model.nodes[0].name = "Fighter";
  model.nodes[0].transform = glm::mat4(1.f);
  model.nodes[1].name = "Root";
  model.nodes[1].parent = 0;
  model.nodes[1].transform = glm::mat4(1.f);
  auto walk = Clip("Walk", 1.f);
  walk.channels.push_back({.nodeIndex = 1, .nodeName = "Root", .positions = {{.0f, {.0f, .0f, .0f}}, {30.f, {.0f, .0f, 2.f}}}});
  model.animations = {Clip("Idle", 1.f), walk, Clip("Punch", 1.f), Clip("Kick", 1.f), Clip("Hit", .5f)};
  return model;
}
constexpr ButtonMask LEFT = 1u << Button::Left;
constexpr ButtonMask RIGHT = 1u << Button::Right;
constexpr ButtonMask PUNCH = 1u << Button::Punch;
constexpr ButtonMask KICK = 1u << Button::Kick;
auto Distance(const DuelSim &sim) -> float {
  return sim.GetFighter(1).position - sim.GetFighter(0).position;
}
auto Step(DuelSim &sim, const ButtonMask first, const ButtonMask second, const int ticks = 1) -> void {
  const std::array<ButtonMask, 2> masks{first, second};
  for (auto tick = 0; tick < ticks; ++tick)
    sim.Advance(sim.GetTick(), masks, false);
}
auto Snapshot(const DuelSim &sim) -> std::vector<std::byte> {
  std::vector<std::byte> bytes;
  sim.Save(bytes);
  return bytes;
}
/// The game as it would have gone with no link at all, with a digest of its state after each tick.
class Traced final : public RollbackGame {
public:
  explicit Traced(const ModelAsset &model)
    : sim(model, RATE) {
    sim.SetLogging(false);
  }
  DuelSim sim;
  std::map<int64_t, uint32_t> trace;
  auto Advance(const int64_t tick, const std::span<const ButtonMask> buttons, const bool replay) -> void override {
    sim.Advance(tick, buttons, replay);
    uint32_t hash = 2166136261u;
    for (const auto byte : Snapshot(sim))
      hash = (hash ^ static_cast<uint32_t>(byte)) * 16777619u;
    trace[tick] = hash;
  }
  auto Save(std::vector<std::byte> &out) const -> void override {
    sim.Save(out);
  }
  auto Load(const std::span<const std::byte> in) -> void override {
    sim.Load(in);
  }
};
} // namespace
TEST(DuelSim, StartsWithTheFiguresFacingEachOther) {
  const auto model = MakeModel();
  DuelSim sim(model, RATE);
  EXPECT_LT(sim.GetFighter(0).position, sim.GetFighter(1).position);
  EXPECT_EQ(sim.GetFighter(0).facing, 1.f);
  EXPECT_EQ(sim.GetFighter(1).facing, -1.f);
  EXPECT_EQ(sim.GetAnimator(0).state, Idle);
}
TEST(DuelSim, WalksTowardsTheOtherWhateverWayItFaces) {
  const auto model = MakeModel();
  DuelSim sim(model, RATE);
  sim.SetLogging(false);
  const auto before = Distance(sim);
  // The keyboard is the same for both, so the second walks forwards by pressing the other key.
  Step(sim, RIGHT, LEFT, 30);
  EXPECT_LT(Distance(sim), before - .5f);
  EXPECT_EQ(sim.GetAnimator(0).state, Walk);
  EXPECT_EQ(sim.GetAnimator(1).state, Walk);
}
TEST(DuelSim, StepsBackFromTheOtherByTheOtherKey) {
  const auto model = MakeModel();
  DuelSim sim(model, RATE);
  sim.SetLogging(false);
  const auto before = Distance(sim);
  Step(sim, LEFT, RIGHT, 30);
  EXPECT_GT(Distance(sim), before + .3f);
  EXPECT_EQ(sim.GetAnimator(0).state, Back);
  EXPECT_EQ(sim.GetAnimator(1).state, Back);
}
TEST(DuelSim, ALandedBlowCountsAndStaggersTheOther) {
  const auto model = MakeModel();
  DuelSim sim(model, RATE);
  sim.SetLogging(false);
  for (auto tick = 0; tick < 600 && Distance(sim) > 1.1f; ++tick)
    Step(sim, RIGHT, 0);
  ASSERT_LE(Distance(sim), 1.1f);
  Step(sim, RIGHT | PUNCH, 0);
  Step(sim, 0, 0, 40);
  EXPECT_EQ(sim.GetFighter(0).hits, 1);
  EXPECT_EQ(sim.GetFighter(1).hits, 0);
  ASSERT_EQ(sim.GetImpacts().size(), 1u);
  EXPECT_TRUE(sim.GetImpacts()[0].hit);
  EXPECT_EQ(sim.GetImpacts()[0].attacker, 0);
}
TEST(DuelSim, ABlowOutOfReachMissesAndIsStillRecorded) {
  const auto model = MakeModel();
  DuelSim sim(model, RATE);
  sim.SetLogging(false);
  Step(sim, PUNCH, 0);
  Step(sim, 0, 0, 40);
  EXPECT_EQ(sim.GetFighter(0).hits, 0);
  ASSERT_EQ(sim.GetImpacts().size(), 1u);
  EXPECT_FALSE(sim.GetImpacts()[0].hit);
}
TEST(DuelSim, ALungeIsTwoTapsForwardsForEitherFigure) {
  const auto model = MakeModel();
  for (const auto player : {0, 1}) {
    DuelSim sim(model, RATE);
    sim.SetLogging(false);
    const auto forward = player == 0 ? RIGHT : LEFT;
    const auto back = player == 0 ? LEFT : RIGHT;
    std::array<ButtonMask, 2> masks{};
    for (const auto pressed : {forward, 0u, forward}) {
      masks[player] = pressed;
      sim.Advance(sim.GetTick(), masks, false);
    }
    EXPECT_EQ(sim.GetAnimator(player).state, Lunge) << "player " << player;
    EXPECT_NE(sim.GetAnimator(1 - player).state, Lunge) << "player " << player;
    // Two taps of the other key are two steps back and not a lunge.
    DuelSim other(model, RATE);
    other.SetLogging(false);
    masks = {};
    for (const auto pressed : {back, 0u, back}) {
      masks[player] = pressed;
      other.Advance(other.GetTick(), masks, false);
    }
    EXPECT_NE(other.GetAnimator(player).state, Lunge) << "player " << player;
  }
}
TEST(DuelSim, TheUnattendedPlayerClosesInAndPunches) {
  const auto model = MakeModel();
  DuelSim sim(model, RATE);
  sim.SetLogging(false);
  // It closes to a little over two units and waits there, and throws a punch only at something
  // within reach, so the player has to walk the rest of the way.
  for (auto tick = 0; tick < 900; ++tick) {
    const std::array<ButtonMask, 2> masks{tick < 120 ? RIGHT : 0u, sim.Think(1)};
    sim.Advance(sim.GetTick(), masks, false);
  }
  EXPECT_LT(Distance(sim), 1.3f);
  EXPECT_GT(sim.GetFighter(1).hits, 0);
}
TEST(DuelSim, ARoundTripThroughASnapshotIsExact) {
  const auto model = MakeModel();
  DuelSim sim(model, RATE);
  sim.SetLogging(false);
  std::vector<std::byte> middle;
  for (int64_t tick = 0; tick < 500; ++tick) {
    if (tick == 200)
      middle = Snapshot(sim);
    Step(sim, ScriptedButtons(0, tick), ScriptedButtons(1, tick));
  }
  const auto end = Snapshot(sim);
  ASSERT_NE(middle, end);
  // Put back to the middle and played on again with the same buttons, it must end in the same place
  // in every byte: anything it forgot to write down would be remembered wrongly here.
  sim.Load(middle);
  EXPECT_EQ(sim.GetTick(), 200);
  for (int64_t tick = 200; tick < 500; ++tick)
    Step(sim, ScriptedButtons(0, tick), ScriptedButtons(1, tick));
  EXPECT_EQ(Snapshot(sim), end);
  EXPECT_GT(sim.GetFighter(0).hits + sim.GetFighter(1).hits, 0) << "the script never landed a blow, so this proved little";
}
TEST(DuelSim, ABlowThrownAgainOnReplayKeepsItsRecord) {
  const auto model = MakeModel();
  DuelSim sim(model, RATE);
  sim.SetLogging(false);
  std::vector<std::byte> start;
  for (int64_t tick = 0; tick < 500; ++tick) {
    if (tick == 100)
      start = Snapshot(sim);
    Step(sim, ScriptedButtons(0, tick), ScriptedButtons(1, tick));
  }
  auto &impacts = sim.GetImpacts();
  ASSERT_FALSE(impacts.empty());
  const auto count = impacts.size();
  for (auto &impact : impacts)
    impact.shown = true;
  sim.Load(start);
  for (int64_t tick = 100; tick < 500; ++tick) {
    const std::array<ButtonMask, 2> masks{ScriptedButtons(0, tick), ScriptedButtons(1, tick)};
    sim.Advance(tick, masks, true);
  }
  EXPECT_EQ(sim.GetImpacts().size(), count);
  for (const auto &impact : sim.GetImpacts())
    if (impact.tick >= 100)
      EXPECT_TRUE(impact.shown) << "tick " << impact.tick;
}
TEST(DuelSim, TwoMachinesPlayingTheSameMatchAgreeUnderBadConditions) {
  const auto model = MakeModel();
  constexpr int64_t TICKS = 1200;
  constexpr int DELAY = 2;
  Traced reference(model);
  for (int64_t tick = 0; tick < TICKS; ++tick) {
    // A button asked for on a tick counts `DELAY` ticks later, and the first ticks have none.
    const std::array<ButtonMask, 2> masks{tick < DELAY ? 0u : ScriptedButtons(0, tick - DELAY), tick < DELAY ? 0u : ScriptedButtons(1, tick - DELAY)};
    reference.Advance(tick, masks, false);
  }
  SimulatedLink link({.latency = 4, .jitter = 3, .loss = .15f, .duplicate = .1f, .seed = 21});
  Traced firstGame(model), secondGame(model);
  RollbackSession first(firstGame, link.First(), {.localPlayer = 0, .inputDelay = DELAY});
  RollbackSession second(secondGame, link.Second(), {.localPlayer = 1, .inputDelay = DELAY});
  for (auto unit = 0; unit < TICKS * 50 && (first.GetTick() < TICKS || second.GetTick() < TICKS); ++unit) {
    if (first.GetTick() < TICKS)
      first.Step(ScriptedButtons(0, first.GetTick()));
    if (second.GetTick() < TICKS)
      second.Step(ScriptedButtons(1, second.GetTick()));
    link.Step();
  }
  ASSERT_EQ(first.GetTick(), TICKS);
  ASSERT_EQ(second.GetTick(), TICKS);
  EXPECT_FALSE(first.IsDesynced());
  EXPECT_FALSE(second.IsDesynced());
  EXPECT_GT(first.GetStats().rollbacks + second.GetStats().rollbacks, 0);
  const auto settled = std::min(first.GetConfirmedTick(), second.GetConfirmedTick());
  EXPECT_GT(settled, TICKS / 2);
  for (int64_t tick = 0; tick <= settled; ++tick) {
    ASSERT_EQ(firstGame.trace.at(tick), reference.trace.at(tick)) << "the first machine's tick " << tick;
    ASSERT_EQ(secondGame.trace.at(tick), reference.trace.at(tick)) << "the second machine's tick " << tick;
  }
  // The comparison says nothing unless the match had something in it to disagree about.
  EXPECT_GT(reference.sim.GetFighter(0).hits + reference.sim.GetFighter(1).hits, 0);
}
TEST(RootSpace, KeepsTheEntitysScaleAndTheNodesBetween) {
  ModelAsset model{AssetID{}};
  model.nodes.resize(3);
  model.nodes[0].name = "Fighter";
  model.nodes[0].parent = -1;
  // A turn and a scale: the turn is the game's to set and is dropped, the scale is kept.
  model.nodes[0].transform = glm::rotate(glm::mat4(1.f), glm::radians(90.f), glm::vec3(0.f, 1.f, 0.f)) * glm::scale(glm::mat4(1.f), glm::vec3(2.f, 3.f, 4.f));
  model.nodes[1].name = "Armature";
  model.nodes[1].parent = 0;
  model.nodes[1].transform = glm::scale(glm::mat4(1.f), glm::vec3(.5f));
  model.nodes[2].name = "Root";
  model.nodes[2].parent = 1;
  model.nodes[2].transform = glm::mat4(1.f);
  const auto space = RootSpace(model, "Root");
  const auto travel = space * glm::vec3(0.f, 0.f, 1.f);
  EXPECT_NEAR(travel.x, 0.f, 1e-5f);
  EXPECT_NEAR(travel.y, 0.f, 1e-5f);
  EXPECT_NEAR(travel.z, 2.f, 1e-5f);
  EXPECT_EQ(RootSpace(model, "Nowhere"), glm::mat3(1.f));
}
