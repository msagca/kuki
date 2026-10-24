#include <cmath>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/trigonometric.hpp>
#include <gtest/gtest.h>
#include <inverse_kinematics.hpp>
using namespace kuki;
namespace {
/// @brief Where the middle joint and the end of a limb are once a solution's turns are made.
struct Limb {
  glm::vec3 mid;
  glm::vec3 end;
};
auto Turned(const glm::vec3 &a, const glm::vec3 &b, const glm::vec3 &c, const TwoBoneTurn &turn) -> Limb {
  const auto bent = b + turn.mid * (c - b);
  return {a + turn.root * (b - a), a + turn.root * (bent - a)};
}
} // namespace
TEST(InverseKinematics, PutsTheEndOnAPointInReachAndKeepsTheBones) {
  const glm::vec3 a{.0f, .0f, .0f}, b{.0f, -1.f, .1f}, c{.0f, -2.f, .0f};
  const glm::vec3 target{.5f, -1.2f, .3f};
  const auto limb = Turned(a, b, c, SolveTwoBone(a, b, c, target, {}));
  EXPECT_NEAR(glm::length(limb.end - target), .0f, 1e-4f);
  EXPECT_NEAR(glm::length(limb.mid - a), glm::length(b - a), 1e-5f);
  EXPECT_NEAR(glm::length(limb.end - limb.mid), glm::length(c - b), 1e-5f);
}
TEST(InverseKinematics, StretchesStraightTowardsAPointOutOfReach) {
  const glm::vec3 a{.0f, .0f, .0f}, b{.0f, -1.f, .1f}, c{.0f, -2.f, .0f};
  const glm::vec3 target{3.f, -4.f, .0f};
  const auto limb = Turned(a, b, c, SolveTwoBone(a, b, c, target, {}));
  const auto span = glm::length(b - a) + glm::length(c - b);
  EXPECT_NEAR(glm::length(limb.end - a), span, span * 1e-3f);
  EXPECT_GT(glm::dot(glm::normalize(limb.end - a), glm::normalize(target - a)), .99999f);
}
TEST(InverseKinematics, BendsAStraightLimbTheWayItIsAsked) {
  // Straight down, with nothing in the limb itself to say which way the joint goes.
  const glm::vec3 a{.0f, .0f, .0f}, b{.0f, -1.f, .0f}, c{.0f, -2.f, .0f};
  const glm::vec3 target{.0f, -1.5f, .0f};
  for (const auto way : {1.f, -1.f}) {
    const auto limb = Turned(a, b, c, SolveTwoBone(a, b, c, target, {.0f, .0f, way}));
    EXPECT_NEAR(glm::length(limb.end - target), .0f, 1e-4f);
    EXPECT_GT(limb.mid.z * way, .5f);
    EXPECT_NEAR(limb.mid.x, .0f, 1e-4f);
  }
}
TEST(InverseKinematics, KeepsTheWayTheLimbAlreadyBendsWithoutBeingTold) {
  const glm::vec3 a{.0f, .0f, .0f}, b{.0f, -1.f, -.2f}, c{.0f, -2.f, .0f};
  const auto limb = Turned(a, b, c, SolveTwoBone(a, b, c, {.0f, -1.6f, .0f}, {}));
  EXPECT_LT(limb.mid.z, -.2f);
}
TEST(InverseKinematics, TurnsTheBentLimbAboutItsLineToFaceTheBend) {
  // Bent forwards and asked to bend sideways: the end stays put and the joint goes round.
  const glm::vec3 a{.0f, .0f, .0f}, b{.0f, -1.f, .3f}, c{.0f, -2.f, .0f};
  const glm::vec3 target{.0f, -1.7f, .0f};
  const auto limb = Turned(a, b, c, SolveTwoBone(a, b, c, target, {1.f, .0f, .0f}));
  EXPECT_NEAR(glm::length(limb.end - target), .0f, 1e-4f);
  EXPECT_GT(limb.mid.x, .3f);
  EXPECT_NEAR(limb.mid.z, .0f, 1e-4f);
}
TEST(InverseKinematics, AimsADirectionAtAPointNoFurtherThanItsLimit) {
  const glm::vec3 forward{.0f, .0f, 1.f};
  const glm::vec3 side{1.f, .0f, .0f};
  EXPECT_GT(glm::dot(SolveAim(forward, side, glm::radians(180.f)) * forward, side), .99999f);
  const auto limited = SolveAim(forward, side, glm::radians(30.f)) * forward;
  EXPECT_NEAR(glm::degrees(std::acos(glm::dot(limited, forward))), 30.f, 1e-2f);
  EXPECT_NEAR(limited.y, .0f, 1e-5f);
  EXPECT_GT(limited.x, .0f);
  // Straight behind is still a turn of a definite size, whichever way round it goes.
  const auto behind = SolveAim(forward, -forward, glm::radians(45.f)) * forward;
  EXPECT_NEAR(glm::degrees(std::acos(glm::dot(behind, forward))), 45.f, 1e-2f);
}
