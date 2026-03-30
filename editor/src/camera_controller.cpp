#define GLM_ENABLE_EXPERIMENTAL
#include <algorithm>
#include <application.hpp>
#include <camera.hpp>
#include <camera_controller.hpp>
#include <glfw_constants.hpp>
#include <glm/detail/type_vec2.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/gtx/norm.hpp>
#include <id.hpp>
using namespace kuki;
CameraController::CameraController(Application &app, EntityID entityId)
  : app(app), entityId(entityId) {}
auto CameraController::Update(const float deltaTime) -> void {
  // FIXME: this doesn't update the camera's rotation until a change is manually done through the UI
  auto dirty = false;
  dirty |= UpdatePosition(deltaTime);
  dirty |= mouselook && UpdateRotation();
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
  camera.Update(); // NOTE: local camera needs to be updated manually; the `RenderingSystem` manages those in the scene
}
auto CameraController::UpdatePosition(const float deltaTime) -> bool {
  static constexpr auto MOVE_THRESHOLD = 1e-6f;
  auto input = app.GetWASDKeys();
  // FIXME: prevent movement if there is a key sequence in progress
  if (glm::length2(input) < MOVE_THRESHOLD)
    input = app.GetArrowKeys();
  if (glm::length2(input) < MOVE_THRESHOLD)
    return false;
  const auto shift = app.GetKey(GLFWConst::KEY_LEFT_SHIFT);
  if (shift)
    settings.boostTime = std::min(settings.boostTime + deltaTime, settings.boostRampUpTime);
  else
    settings.boostTime = std::max(0.f, settings.boostTime - deltaTime * (settings.boostRampUpTime / settings.boostRampDownTime));
  settings.moveBoost = 1.f + (settings.moveBoostMax - 1.f) * (settings.boostTime / settings.boostRampUpTime);
  const auto velocity = settings.moveSpeed * settings.moveBoost * deltaTime;
  camera.position += (camera.forward * input.y + camera.right * input.x) * velocity;
  return true;
}
auto CameraController::UpdateRotation() -> bool {
  static constexpr auto MOVE_THRESHOLD = 1e-6f;
  const auto &mousePos = app.GetMousePosition();
  if (mouseEnter)
    mouseLast = mousePos;
  mouseEnter = false;
  glm::vec2 mouseDiff{};
  mouseDiff.x = (mousePos.x - mouseLast.x) * settings.mouseSensitivity;
  mouseDiff.y = (mouseLast.y - mousePos.y) * settings.mouseSensitivity; // NOTE: y is inverted because (0,0) is the northwest corner
  mouseLast = mousePos;
  if (glm::length2(mouseDiff) < MOVE_THRESHOLD)
    return false;
  const auto yaw = glm::angleAxis(-mouseDiff.x, glm::vec3(0.f, 1.f, 0.f));
  const auto pitch = glm::angleAxis(mouseDiff.y, glm::vec3(1.f, 0.f, 0.f));
  camera.rotation = yaw * camera.rotation * pitch;
  return true;
}
