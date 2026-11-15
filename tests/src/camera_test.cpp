#include <bounding_box.hpp>
#include <camera.hpp>
#include <engine_archetypes.hpp>
#include <entity_manager.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_float4.hpp>
#include <glm/geometric.hpp>
#include <glm/gtx/quaternion.hpp>
#include <gtest/gtest.h>
#include <light.hpp>
#include <transform.hpp>
#include <world.hpp>
using namespace kuki;
namespace {
auto Declare(EntityManager &manager) -> void {
  for (const auto &spec : EngineWorld::Specs())
    manager.Declare(spec);
}
auto ExpectNear(const glm::vec3 &actual, const glm::vec3 &expected, const char *what) -> void {
  EXPECT_LT(glm::length(actual - expected), 1e-4f) << what << ": (" << actual.x << ", " << actual.y << ", " << actual.z << ")";
}
auto Box(const glm::vec3 &centre) -> BoundingBox {
  return {.min = centre - glm::vec3(.5f), .max = centre + glm::vec3(.5f)};
}
} // namespace
TEST(Camera, LookRotationFacesTheDirectionGiven) {
  for (const auto direction : {glm::vec3(1.f, .0f, .0f), glm::vec3(-.35f, -1.f, -.45f), glm::vec3(.0f, -1.f, .0f), glm::vec3(.0f, 1.f, .0f)})
    ExpectNear(WorldForward(glm::toMat4(LookRotation(direction))), glm::normalize(direction), "forward");
  ExpectNear(WorldForward(glm::toMat4(LookRotation(glm::vec3(.0f)))), {.0f, .0f, -1.f}, "no direction is no rotation");
}
TEST(Camera, SeesFromItsEntitysPlace) {
  EntityManager manager;
  Declare(manager);
  // Hung under something that is not at the origin, so a camera that read only its own transform,
  // or kept a place of its own, would be somewhere else.
  const auto rig = manager.Create<EmptyNode>("rig");
  const auto entity = manager.Create<CameraObject>("camera");
  ASSERT_TRUE(manager.AddChild(rig, entity));
  manager.GetComponent<Transform>(rig)->position = {.0f, 2.f, .0f};
  auto *place = manager.GetComponent<Transform>(entity);
  place->position = {.0f, .0f, 5.f};
  place->rotation = LookRotation({.0f, .0f, -1.f});
  manager.MarkTransformDirty(rig);
  manager.UpdateComponents<Transform>();
  auto *camera = manager.GetComponent<Camera>(entity);
  camera->transform = ComputeCameraTransform(*camera, manager.GetComponent<WorldMatrix>(entity)->matrix);
  ExpectNear(camera->transform.position, {.0f, 2.f, 5.f}, "position");
  ExpectNear(glm::vec3(camera->transform.view * glm::vec4(.0f, 2.f, 5.f, 1.f)), glm::vec3(.0f), "the eye in view space");
  EXPECT_TRUE(IntersectsFrustum(*camera, Box({.0f, 2.f, .0f}))) << "in front of it";
  EXPECT_FALSE(IntersectsFrustum(*camera, Box({.0f, 2.f, 10.f}))) << "behind it";
}
TEST(Camera, FramingSeesTheWholeBox) {
  Camera camera;
  camera.farPlane = 1.f;
  const auto box = Box({3.f, .0f, .0f});
  const auto framing = Frame(camera, box, {3.f, .0f, .0f});
  camera.farPlane = framing.farPlane;
  camera.transform = ComputeCameraTransform(camera, glm::translate(glm::mat4(1.f), framing.position) * glm::toMat4(framing.rotation));
  EXPECT_GT(framing.farPlane, 1.f) << "pushed out to reach the box";
  for (const auto corner : {box.min, box.max})
    EXPECT_TRUE(IntersectsFrustum(camera, Box(corner))) << "corner (" << corner.x << ", " << corner.y << ", " << corner.z << ")";
}
