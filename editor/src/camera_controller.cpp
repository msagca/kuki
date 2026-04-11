#define GLM_ENABLE_EXPERIMENTAL
#include <algorithm>
#include <application.hpp>
#include <camera.hpp>
#include <camera_controller.hpp>
#include <entity_manager.hpp>
#include <glfw_constants.hpp>
#include <glm/detail/type_vec2.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <id.hpp>
#include <imgui.h>
//
#include <GLFW/glfw3.h>
using namespace kuki;
CameraController::CameraController()
  : Script(std::in_place_type<CameraController>) {}
auto CameraController::CloneTo(EntityManager &entityManager, const EntityID id) const -> void {
  auto script = entityManager.AddComponent<CameraController>(id);
  *script = *this;
}
auto CameraController::Display() const -> void {
  // TODO: make these configurable
  if (ImGui::CollapsingHeader("Camera Controller")) {
    ImGui::Text("Move: WASD or Arrow Keys");
    ImGui::Text("Boost: Hold Shift");
    ImGui::Text("Look: Hold Right Mouse Button and Move Mouse");
  }
}
auto CameraController::GetProjection() const -> const glm::mat4 & {
  return camera.transform.projection;
}
auto CameraController::GetType() const -> const CameraType & {
  return camera.type;
}
auto CameraController::GetView() const -> const glm::mat4 & {
  return camera.transform.view;
}
auto CameraController::Start(Application &app) -> void {
  app.RegisterInputAction(GLFW_MOUSE_BUTTON_RIGHT, [this]() {
    mouselook = true;
    mouseEnter = true;
  });
  app.RegisterInputAction(GLFW_MOUSE_BUTTON_RIGHT, [this]() { mouselook = false; }, false);
}
auto CameraController::Update(Application &app) -> void {
  auto dirty = false;
  dirty |= UpdatePosition(app);
  dirty |= mouselook && UpdateRotation(app);
  camera.dirty += dirty;
  auto cameraPtr = app.GetEntityComponent<Camera>(entityId);
  if (cameraPtr) {
    if (cameraMissing || cameraPtr->dirty > camera.dirty)
      camera = *cameraPtr;
    else if (camera.dirty > cameraPtr->dirty)
      *cameraPtr = camera;
    // NOTE: if `dirty` wraps around, this will temporarily misbehave
    cameraMissing = false;
  } else
    cameraMissing = true;
  camera.Update();
}
auto CameraController::UpdatePosition(Application &app) -> bool {
  static constexpr auto EPSILON = 1e-6f;
  auto input = app.GetWASDKeys();
  // FIXME: prevent movement if there is a key sequence in progress
  if (glm::length2(input) < EPSILON)
    input = app.GetArrowKeys();
  if (glm::length2(input) < EPSILON)
    return false;
  const auto shift = app.GetKey(GLFWConst::KEY_LEFT_SHIFT);
  const auto deltaTime = app.DeltaTime();
  if (shift)
    settings.boostTime = std::min(settings.boostTime + deltaTime, settings.boostRampUpTime);
  else
    settings.boostTime = std::max(0.f, settings.boostTime - deltaTime * (settings.boostRampUpTime / settings.boostRampDownTime));
  settings.moveBoost = 1.f + (settings.moveBoostMax - 1.f) * (settings.boostTime / settings.boostRampUpTime);
  const auto velocity = settings.moveSpeed * settings.moveBoost * deltaTime;
  camera.position += (camera.forward * input.y + camera.right * input.x) * velocity;
  return true;
}
auto CameraController::UpdateRotation(Application &app) -> bool {
  static constexpr auto EPSILON = 1e-6f;
  static constexpr auto WORLD_UP = glm::vec3(0.f, 1.f, 0.f);
  static constexpr auto WORLD_RIGHT = glm::vec3(1.f, 0.f, 0.f);
  const auto mousePos = app.GetMousePosition();
  if (mouseEnter)
    mouseLast = mousePos;
  mouseEnter = false;
  glm::vec2 mouseDiff{};
  mouseDiff.x = (mousePos.x - mouseLast.x) * settings.mouseSensitivity;
  mouseDiff.y = (mouseLast.y - mousePos.y) * settings.mouseSensitivity; // NOTE: y is inverted because (0,0) is the northwest corner
  mouseLast = mousePos;
  if (glm::length2(mouseDiff) < EPSILON)
    return false;
  const auto yaw = glm::angleAxis(-mouseDiff.x, WORLD_UP);
  const auto rotation = yaw * camera.rotation;
  const auto right = glm::normalize(rotation * WORLD_RIGHT);
  const auto pitch = glm::angleAxis(mouseDiff.y, right);
  camera.rotation = glm::normalize(pitch * rotation);
  // TODO: remove roll if non-zero (e.g., set via UI)
  return true;
}
