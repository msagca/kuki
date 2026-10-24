#include <animation_system.hpp>
#include <animator.hpp>
#include <asset.hpp>
#include <gtest/gtest.h>
#include <model_asset.hpp>
#include <string>
#include <vector>
using namespace kuki;
namespace {
constexpr int RATE = 60;
/// @brief A model of two nodes and two clips, each a second long at thirty of its own ticks a second.
///
/// `Walk` carries the root two units along Z and turns the arm; `Punch` moves neither the root nor,
/// deliberately, anything but the arm, so there is a bone one clip moves and the other does not.
auto MakeModel() -> ModelAsset {
  ModelAsset model{AssetID{}};
  model.nodes.resize(2);
  model.nodes[0].name = "Root";
  model.nodes[0].transform = glm::mat4(1.f);
  model.nodes[1].name = "Arm";
  model.nodes[1].transform = glm::mat4(1.f);
  model.nodes[1].transform[3] = {.0f, 1.f, .0f, 1.f};
  AnimationClip walk;
  walk.name = "Armature|Walk";
  walk.duration = 30.f;
  walk.ticksPerSecond = 30.f;
  walk.channels.push_back({.nodeIndex = 0, .nodeName = "Root", .positions = {{.0f, {.0f, .5f, .0f}}, {30.f, {.0f, .5f, 2.f}}}});
  AnimationClip punch;
  punch.name = "Punch";
  punch.duration = 30.f;
  punch.ticksPerSecond = 30.f;
  punch.channels.push_back({.nodeIndex = 1, .nodeName = "Arm", .positions = {{.0f, {.0f, 1.f, .0f}}, {30.f, {.0f, 1.f, 4.f}}}});
  model.animations = {walk, punch};
  return model;
}
auto MakeAnimator() -> Animator {
  Animator animator;
  animator.rootNode = "Root";
  animator.states.push_back({.name = "walk", .clip = "Walk", .loop = true, .events = {{15, "step"}}});
  animator.states.push_back({.name = "punch", .clip = "Punch", .fade = 4, .next = "walk", .events = {{0, "start"}, {20, "hit"}}});
  animator.states.push_back({.name = "stay", .clip = "Punch"});
  return animator;
}
} // namespace
TEST(Animator, EntersItsFirstStateAndFindsAPrefixedClip) {
  const auto model = MakeModel();
  auto animator = MakeAnimator();
  StepAnimator(animator, model, RATE);
  EXPECT_EQ(animator.GetStateName(), "walk");
  EXPECT_EQ(animator.states[0].clipIndex, 0);
  EXPECT_EQ(animator.stateTicks, 1);
}
TEST(Animator, FiresAnEventOnItsTickEachTimeRoundALoop) {
  const auto model = MakeModel();
  auto animator = MakeAnimator();
  std::vector<int> fired;
  for (auto tick = 0; tick < 180; ++tick) {
    StepAnimator(animator, model, RATE);
    if (animator.Fired("step"))
      fired.push_back(tick);
  }
  EXPECT_EQ(fired, (std::vector<int>{15, 75, 135}));
}
TEST(Animator, GoesOnToTheNextStateWhenAClipEnds) {
  const auto model = MakeModel();
  auto animator = MakeAnimator();
  StepAnimator(animator, model, RATE);
  ASSERT_TRUE(animator.Play("punch"));
  auto start = -1, hit = -1, back = -1;
  for (auto tick = 0; tick < 70 && back < 0; ++tick) {
    StepAnimator(animator, model, RATE);
    if (animator.Fired("start"))
      start = tick;
    if (animator.Fired("hit"))
      hit = tick;
    if (animator.GetStateName() == "walk")
      back = tick;
  }
  EXPECT_EQ(start, 0);
  EXPECT_EQ(hit, 20);
  EXPECT_EQ(back, 59);
  EXPECT_FALSE(animator.finished);
}
TEST(Animator, HoldsAtTheEndOfAStateWithNothingAfterIt) {
  const auto model = MakeModel();
  auto animator = MakeAnimator();
  animator.Play("stay");
  for (auto tick = 0; tick < 59; ++tick)
    StepAnimator(animator, model, RATE);
  EXPECT_FALSE(animator.finished);
  StepAnimator(animator, model, RATE);
  EXPECT_TRUE(animator.finished);
  EXPECT_EQ(animator.GetStateName(), "stay");
}
TEST(Animator, RestartsOnlyWhenAsked) {
  const auto model = MakeModel();
  auto animator = MakeAnimator();
  for (auto tick = 0; tick < 10; ++tick)
    StepAnimator(animator, model, RATE);
  EXPECT_TRUE(animator.Play("walk"));
  EXPECT_EQ(animator.stateTicks, 10);
  EXPECT_TRUE(animator.Play("walk", true));
  EXPECT_EQ(animator.stateTicks, 0);
  EXPECT_FALSE(animator.Play("nothing"));
}
TEST(Animator, ReportsTheRootsTravelWithoutGoingBackRoundALoop) {
  const auto model = MakeModel();
  auto animator = MakeAnimator();
  auto travelled = .0f;
  for (auto tick = 0; tick < 120; ++tick) {
    StepAnimator(animator, model, RATE);
    EXPECT_GE(animator.rootDelta.z, .0f);
    EXPECT_EQ(animator.rootDelta.y, .0f);
    travelled += animator.rootDelta.z;
  }
  EXPECT_NEAR(travelled, 4.f, 1e-3f);
}
TEST(Animator, HoldsTheRootInPlaceInThePose) {
  const auto model = MakeModel();
  auto animator = MakeAnimator();
  for (auto tick = 0; tick < 31; ++tick)
    StepAnimator(animator, model, RATE);
  PoseAnimator(animator, model, RATE, .0f);
  ASSERT_EQ(animator.pose.size(), 2u);
  ASSERT_TRUE(animator.pose[0].set);
  EXPECT_NEAR(animator.pose[0].position.z, .0f, 1e-4f);
  EXPECT_NEAR(animator.pose[0].position.y, .5f, 1e-4f);
  EXPECT_FALSE(animator.pose[1].set);
}
TEST(Animator, DrawsBetweenTheLastTwoTicks) {
  const auto model = MakeModel();
  auto animator = MakeAnimator();
  animator.Play("stay");
  for (auto tick = 0; tick < 31; ++tick)
    StepAnimator(animator, model, RATE);
  PoseAnimator(animator, model, RATE, .0f);
  EXPECT_NEAR(animator.pose[1].position.z, 2.f, 1e-3f);
  PoseAnimator(animator, model, RATE, .5f);
  EXPECT_NEAR(animator.pose[1].position.z, 2.f + 4.f / 120.f, 1e-3f);
}
TEST(Animator, FadesFromThePoseThatWasShowing) {
  const auto model = MakeModel();
  auto animator = MakeAnimator();
  animator.Play("stay");
  for (auto tick = 0; tick < 60; ++tick)
    StepAnimator(animator, model, RATE);
  PoseAnimator(animator, model, RATE, 1.f);
  EXPECT_NEAR(animator.pose[1].position.z, 4.f, 1e-3f);
  // Into a state whose clip does not move the arm at all: it has to come back to where the model
  // has it, over the fade, rather than stay out or snap.
  animator.Play("walk");
  EXPECT_EQ(animator.fadeTicks, 6);
  StepAnimator(animator, model, RATE);
  PoseAnimator(animator, model, RATE, .0f);
  EXPECT_NEAR(animator.pose[1].position.z, 4.f, 1e-3f);
  for (auto tick = 0; tick < 3; ++tick)
    StepAnimator(animator, model, RATE);
  PoseAnimator(animator, model, RATE, .0f);
  EXPECT_NEAR(animator.pose[1].position.z, 2.f, 1e-3f);
  for (auto tick = 0; tick < 3; ++tick)
    StepAnimator(animator, model, RATE);
  PoseAnimator(animator, model, RATE, .0f);
  EXPECT_FALSE(animator.pose[1].set);
}
TEST(Animator, PlaysAClipFromAnotherModelByBoneName) {
  const auto model = MakeModel();
  // A second file that holds a clip and a skeleton and no mesh: the same bones under the same
  // names, listed the other way round and with one this model does not have, and a clip called
  // what its tool calls every clip.
  ModelAsset clips{AssetID{}};
  clips.nodes.resize(3);
  clips.nodes[0].name = "Arm";
  clips.nodes[1].name = "Tail";
  clips.nodes[2].name = "Root";
  AnimationClip wave;
  wave.name = "mixamo.com";
  wave.duration = 30.f;
  wave.ticksPerSecond = 30.f;
  wave.channels.push_back({.nodeIndex = 0, .nodeName = "Arm", .positions = {{.0f, {.0f, 1.f, .0f}}, {30.f, {.0f, 1.f, 6.f}}}});
  wave.channels.push_back({.nodeIndex = 1, .nodeName = "Tail", .positions = {{.0f, {9.f, 9.f, 9.f}}}});
  wave.channels.push_back({.nodeIndex = 2, .nodeName = "Root", .positions = {{.0f, {.0f, .5f, .0f}}, {30.f, {.0f, .5f, 3.f}}}});
  clips.animations = {wave};
  const auto source = MakeBuiltInAssetID("test/clips");
  const ModelLookup lookup = [&](const AssetID id) { return id == source ? &clips : nullptr; };
  Animator animator;
  animator.rootNode = "Root";
  animator.states.push_back({.name = "wave", .source = source, .loop = true});
  auto travelled = .0f;
  for (auto tick = 0; tick < 31; ++tick) {
    StepAnimator(animator, model, RATE, lookup);
    travelled += animator.rootDelta.z;
  }
  EXPECT_EQ(animator.states[0].clipIndex, 0);
  EXPECT_EQ(animator.states[0].nodes, (std::vector<int>{1, -1, 0}));
  EXPECT_NEAR(travelled, 1.5f + 3.f / 60.f, 1e-3f);
  PoseAnimator(animator, model, RATE, .0f, lookup);
  ASSERT_EQ(animator.pose.size(), 2u);
  EXPECT_NEAR(animator.pose[1].position.z, 3.f, 1e-3f);
  EXPECT_NEAR(animator.pose[0].position.z, .0f, 1e-4f);
  // Without anything to find the other model by, the state has no clip and nothing moves.
  Animator stranded;
  stranded.states.push_back({.name = "wave", .source = source, .loop = true});
  StepAnimator(stranded, model, RATE);
  EXPECT_EQ(stranded.stateTicks, 0);
}
TEST(Animator, BlendsTwoGaitsInStepAndTravelsBetweenThem) {
  const auto model = MakeModel();
  // A walk of one second that goes two units and a run of half a second that goes three, as two
  // clips of one model. Both swing the arm out and back once a cycle, to different heights.
  ModelAsset gaits{AssetID{}};
  gaits.nodes.resize(2);
  gaits.nodes[0].name = "Root";
  gaits.nodes[1].name = "Arm";
  const auto Gait = [](const char *name, const float frames, const float travel, const float swing) {
    AnimationClip clip;
    clip.name = name;
    clip.duration = frames;
    clip.ticksPerSecond = 30.f;
    clip.channels.push_back({.nodeIndex = 0, .nodeName = "Root", .positions = {{.0f, {.0f, .5f, .0f}}, {frames, {.0f, .5f, travel}}}});
    clip.channels.push_back({.nodeIndex = 1, .nodeName = "Arm", .positions = {{.0f, {.0f, 1.f, .0f}}, {frames * .5f, {.0f, swing, .0f}}, {frames, {.0f, 1.f, .0f}}}});
    return clip;
  };
  gaits.animations = {Gait("Walk", 30.f, 2.f, 2.f), Gait("Run", 15.f, 3.f, 4.f)};
  const auto source = MakeBuiltInAssetID("test/gaits");
  const ModelLookup lookup = [&](const AssetID id) { return id == source ? &gaits : nullptr; };
  const auto Travel = [&](const float value, const int ticks) {
    Animator animator;
    animator.rootNode = "Root";
    animator.states.push_back({.name = "move", .loop = true, .blend = {{.source = source, .clip = "Walk", .at = 1.f}, {.source = source, .clip = "Run", .at = 3.f}}});
    animator.SetBlend("move", value);
    auto travelled = .0f;
    for (auto tick = 0; tick < ticks; ++tick) {
      StepAnimator(animator, model, RATE, lookup);
      travelled += animator.rootDelta.z;
    }
    return travelled;
  };
  // At either end it is the one gait: two units a second walking, six running. Below and above the
  // ends it stays there. Half way it is a cycle of three quarters of a second going two and a half.
  EXPECT_NEAR(Travel(1.f, 120), 4.f, 2e-2f);
  EXPECT_NEAR(Travel(3.f, 120), 12.f, 2e-2f);
  EXPECT_NEAR(Travel(-5.f, 120), 4.f, 2e-2f);
  EXPECT_NEAR(Travel(9.f, 120), 12.f, 2e-2f);
  EXPECT_NEAR(Travel(2.f, 90), 5.f, 2e-2f);
  // A quarter of the way through the mixed cycle the arm is half way up in both, so half way up
  // their mix: both clips are at the same point of the stride, whatever their own lengths.
  Animator animator;
  animator.states.push_back({.name = "move", .loop = true, .blend = {{.source = source, .clip = "Walk", .at = 1.f}, {.source = source, .clip = "Run", .at = 3.f}}});
  animator.SetBlend("move", 2.f);
  for (auto tick = 0; tick < 11; ++tick)
    StepAnimator(animator, model, RATE, lookup);
  EXPECT_NEAR(animator.phase, 11.f / 45.f, 1e-4f);
  PoseAnimator(animator, model, RATE, 1.f, lookup);
  EXPECT_NEAR(animator.pose[1].position.y, 1.f + 2.f * (11.f / 45.f) * 2.f, 1e-3f);
}
