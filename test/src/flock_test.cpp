#include <algorithm>
#include <flock.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/geometric.hpp>
#include <gtest/gtest.h>
using namespace kuki;
namespace {
constexpr float STEP = 1.f / 60.f;
auto Fly(Flock &flock, const int steps) -> void {
  for (auto i = 0; i < steps; ++i)
    flock.Step(STEP);
}
} // namespace
TEST(Flock, KeepsItsSpeedAndHeight) {
  FlockSettings settings;
  Flock flock(24, {.0f, 10.f, .0f}, settings, 3);
  flock.SetGround([](const float x, const float) { return x > .0f ? 4.f : .0f; });
  Fly(flock, 60 * 20);
  for (const auto &boid : flock.GetBoids()) {
    const auto speed = glm::length(boid.velocity);
    EXPECT_GE(speed, settings.minSpeed - 1e-3f);
    EXPECT_LE(speed, settings.maxSpeed + 1e-3f);
    const auto ground = boid.position.x > .0f ? 4.f : .0f;
    // The band is held by a spring, so a boid may overshoot it a little.
    EXPECT_GT(boid.position.y - ground, settings.minHeight - 3.f);
    EXPECT_LT(boid.position.y - ground, settings.maxHeight + 3.f);
  }
}
TEST(Flock, StaysTogetherAndFollowsItsGoal) {
  Flock flock(30, {.0f, 10.f, .0f}, {}, 8);
  flock.SetGoal({60.f, 10.f, .0f});
  Fly(flock, 60 * 15);
  const auto centre = flock.Centre();
  EXPECT_LT(glm::distance(glm::vec2(centre.x, centre.z), glm::vec2(60.f, .0f)), 25.f);
  auto spread = .0f;
  for (const auto &boid : flock.GetBoids())
    spread = std::max(spread, glm::distance(boid.position, centre));
  EXPECT_LT(spread, 30.f);
}
TEST(Flock, IsTheSameForTheSameSeed) {
  Flock a(12, {}, {}, 5);
  Flock b(12, {}, {}, 5);
  Fly(a, 300);
  Fly(b, 300);
  for (size_t i = 0; i < a.GetBoids().size(); ++i)
    EXPECT_EQ(a.GetBoids()[i].position, b.GetBoids()[i].position);
}
