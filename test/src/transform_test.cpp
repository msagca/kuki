#include <glm/ext/quaternion_float.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <serdes.hpp>
#include <transform.hpp>
using namespace kuki;
namespace {
auto IsIdentity(const glm::quat &q) -> bool {
  return q.w == 1.f && q.x == .0f && q.y == .0f && q.z == .0f;
}
} // namespace
TEST(Transform, DefaultRotationIsTheIdentity) {
  // `glm::quat{}` is all zeros: no rotation at all, which turns every vector to nothing.
  const Transform transform{};
  EXPECT_TRUE(IsIdentity(transform.rotation));
  const glm::vec3 v{1.f, 2.f, 3.f};
  EXPECT_LT(glm::length(transform.rotation * v - v), 1e-6f);
}
TEST(Transform, SavedZeroRotationReadsAsTheIdentity) {
  // What every scene saved before the default was fixed holds for an entity never turned.
  const auto saved = nlohmann::json::parse(R"({"w": 0.0, "x": 0.0, "y": 0.0, "z": 0.0})");
  EXPECT_TRUE(IsIdentity(saved.get<glm::quat>()));
  // Any other rotation is read as written.
  const auto turned = nlohmann::json::parse(R"({"w": 0.0, "x": 1.0, "y": 0.0, "z": 0.0})");
  const auto read = turned.get<glm::quat>();
  EXPECT_EQ(read.w, .0f);
  EXPECT_EQ(read.x, 1.f);
}
