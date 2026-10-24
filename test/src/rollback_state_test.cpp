#include <animation_system.hpp>
#include <animator.hpp>
#include <animator_state.hpp>
#include <asset.hpp>
#include <gesture.hpp>
#include <gtest/gtest.h>
#include <model_asset.hpp>
#include <rollback.hpp>
#include <string>
#include <vector>
using namespace kuki;
namespace {
auto Masks(const int count) -> std::vector<ButtonMask> {
  std::vector<ButtonMask> masks;
  uint32_t state = 7;
  for (auto tick = 0; tick < count; ++tick) {
    // Held for a few ticks at a time, as fingers are.
    if (tick % 4 == 0)
      state = state * 1664525u + 1013904223u;
    masks.push_back(state >> 20 & 7u);
  }
  return masks;
}
} // namespace
TEST(GestureRecognizerMemory, PutBackItGivesTheSameGesturesFromThere) {
  const std::vector<std::string> names{"left", "right", "punch"};
  GestureRecognizer recognizer;
  recognizer.Add("lunge", "right right", names, 12);
  recognizer.Add("punch", "punch", names);
  recognizer.Add("charge", "punch*10 -punch", names);
  const auto masks = Masks(600);
  GestureRecognizer::Memory remembered;
  std::vector<std::vector<GestureID>> first;
  for (size_t tick = 0; tick < masks.size(); ++tick) {
    if (tick == 250)
      remembered = recognizer.GetMemory();
    const auto matched = recognizer.Advance(masks[tick]);
    if (tick >= 250)
      first.emplace_back(matched.begin(), matched.end());
  }
  recognizer.SetMemory(remembered);
  std::vector<std::vector<GestureID>> second;
  for (size_t tick = 250; tick < masks.size(); ++tick) {
    const auto matched = recognizer.Advance(masks[tick]);
    second.emplace_back(matched.begin(), matched.end());
  }
  EXPECT_EQ(first, second);
  size_t found = 0;
  for (const auto &matched : first)
    found += matched.size();
  EXPECT_GT(found, 0u) << "nothing was recognised, so this proved little";
}
TEST(GestureRecognizerMemory, ForgetsWhatCameAfter) {
  const std::vector<std::string> names{"left", "right", "punch"};
  GestureRecognizer recognizer;
  recognizer.Add("lunge", "right right", names, 12);
  recognizer.Advance(2);
  const auto memory = recognizer.GetMemory();
  recognizer.Advance(0);
  recognizer.Advance(2);
  recognizer.SetMemory(memory);
  EXPECT_EQ(recognizer.GetHeld(), 2u);
  recognizer.Advance(0);
  EXPECT_EQ(recognizer.Advance(2).size(), 1u);
}
TEST(AnimatorTick, PutBackItStepsOnExactlyAsItDidBefore) {
  ModelAsset model{AssetID{}};
  model.nodes.resize(2);
  model.nodes[0].name = "Root";
  model.nodes[0].transform = glm::mat4(1.f);
  model.nodes[1].name = "Arm";
  model.nodes[1].transform = glm::mat4(1.f);
  AnimationClip walk;
  walk.name = "Walk";
  walk.duration = 30.f;
  walk.ticksPerSecond = 30.f;
  walk.channels.push_back({.nodeIndex = 0, .nodeName = "Root", .positions = {{.0f, {.0f, .0f, .0f}}, {30.f, {.0f, .0f, 2.f}}}});
  AnimationClip punch;
  punch.name = "Punch";
  punch.duration = 15.f;
  punch.ticksPerSecond = 30.f;
  model.animations = {walk, punch};
  Animator animator;
  animator.rootNode = "Root";
  animator.states.push_back({.name = "walk", .clip = "Walk", .loop = true, .events = {{15, "step"}}});
  animator.states.push_back({.name = "punch", .clip = "Punch", .next = "walk", .events = {{5, "hit"}}});
  const auto run = [&](const int from, const int to) {
    std::vector<std::byte> trace;
    for (auto tick = from; tick < to; ++tick) {
      if (tick % 70 == 30)
        animator.Play("punch", true);
      StepAnimator(animator, model, 60);
      StateWriter writer(trace);
      SaveAnimatorTick(writer, animator);
    }
    return trace;
  };
  run(0, 85);
  std::vector<std::byte> middle;
  StateWriter writer(middle);
  SaveAnimatorTick(writer, animator);
  const auto first = run(85, 300);
  StateReader reader(middle);
  LoadAnimatorTick(reader, animator);
  EXPECT_TRUE(reader.Ok());
  const auto second = run(85, 300);
  EXPECT_EQ(first, second);
  EXPECT_FALSE(first.empty());
}
