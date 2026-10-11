#include <animation_system.hpp>
#include <animator.hpp>
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
