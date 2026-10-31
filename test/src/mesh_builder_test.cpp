#include <glm/geometric.hpp>
#include <gtest/gtest.h>
#include <mesh_builder.hpp>
using namespace kuki;
namespace {
/// @brief Whether every triangle's normal is its own face's, out of the side it was asked to face.
auto FacesOutFrom(const Mesh &mesh, const glm::vec3 &centre) -> bool {
  for (size_t i = 0; i + 2 < mesh.vertices.size(); i += 3) {
    const auto &a = mesh.vertices[i];
    const auto &b = mesh.vertices[i + 1];
    const auto &c = mesh.vertices[i + 2];
    const auto face = glm::normalize(glm::cross(b.position - a.position, c.position - a.position));
    if (glm::dot(face, a.normal) < .999f)
      return false;
    if (glm::dot(face, (a.position + b.position + c.position) / 3.f - centre) <= .0f)
      return false;
  }
  return true;
}
} // namespace
TEST(MeshBuilder, BuildsClosedSolidsFacingOut) {
  MeshBuilder builder;
  builder.Box({-1.f, -1.f, -1.f}, {1.f, 1.f, 1.f});
  EXPECT_EQ(builder.VertexCount(), 36u);
  EXPECT_TRUE(FacesOutFrom(builder.GetMesh(), {}));
  builder.Take();
  builder.Frustum({.0f, -.5f, .0f}, 1.f, .5f, 1.f, 6);
  EXPECT_TRUE(FacesOutFrom(builder.GetMesh(), {}));
  builder.Take();
  builder.Rock({}, glm::vec3(1.f), .2f, 3u, 1);
  EXPECT_EQ(builder.VertexCount(), 80u * 3u);
  EXPECT_TRUE(FacesOutFrom(builder.GetMesh(), {}));
}
TEST(MeshBuilder, OrientsFacesAsAsked) {
  MeshBuilder builder;
  // Wound clockwise seen from above, and still faced up.
  builder.Oriented({0, 0, 0}, {0, 0, 1}, {1, 0, 0}, {0, 1, 0});
  ASSERT_EQ(builder.VertexCount(), 3u);
  EXPECT_GT(builder.GetMesh().vertices[0].normal.y, .99f);
  // A triangle with no area is left out rather than given a normal that is not a number.
  builder.Triangle({0, 0, 0}, {1, 0, 0}, {2, 0, 0});
  EXPECT_EQ(builder.VertexCount(), 3u);
}
TEST(MeshBuilder, PlacesThroughItsTransform) {
  MeshBuilder builder;
  builder.SetTransform(glm::mat4(1.f) * glm::mat4(glm::vec4(1, 0, 0, 0), glm::vec4(0, 1, 0, 0), glm::vec4(0, 0, 1, 0), glm::vec4(5, 0, 0, 1)));
  builder.Box({-1.f, -1.f, -1.f}, {1.f, 1.f, 1.f});
  const auto bounds = builder.Bounds();
  EXPECT_FLOAT_EQ(bounds.min.x, 4.f);
  EXPECT_FLOAT_EQ(bounds.max.x, 6.f);
  EXPECT_TRUE(FacesOutFrom(builder.GetMesh(), {5.f, .0f, .0f}));
}
