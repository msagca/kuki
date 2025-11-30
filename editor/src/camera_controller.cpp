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
CameraController::CameraController(Application& app, ID entityId)
  : app(app), entityId(entityId) {}
void CameraController::Update(float deltaTime) {
  static auto firstEnter = true;
  static glm::vec2 mousePos;
  static glm::vec2 mouseLast;
  auto positionDirty = false;
  auto rotationDirty = false;
  if (app.GetButtonDown(GLFWConst::MOUSE_BUTTON_RIGHT)) {
    mousePos = app.GetMousePos();
    if (firstEnter)
      mouseLast = mousePos;
    firstEnter = false;
    glm::vec2 mouseDiff{};
    mouseDiff.x = (mousePos.x - mouseLast.x) * mouseSensitivity;
    mouseDiff.y = (mouseLast.y - mousePos.y) * mouseSensitivity; // NOTE: y is inverted because position (0,0) is at the top-left corner
    mouseLast = mousePos;
    if (mouselook)
      rotationDirty = UpdateRotation(mouseDiff);
  } else
    firstEnter = true;
  positionDirty = UpdatePosition(deltaTime);
  auto cameraPtr = app.GetEntityComponent<Camera>(entityId);
  if (cameraPtr) {
    // prioritize local changes over editor changes
    if (positionDirty)
      cameraPtr->position = camera.position;
    if (rotationDirty)
      cameraPtr->rotation = camera.rotation;
    cameraPtr->aspectRatio = camera.aspectRatio; // aspect ratio is only allowed to be updated by the rendering system, not through the editor UI
    cameraPtr->positionDirty |= positionDirty;
    cameraPtr->rotationDirty |= rotationDirty;
    cameraPtr->uboDirty |= positionDirty || rotationDirty;
    // NOTE: `Camera` components are updated by a relevant system (if marked dirty); do not call `camera->Update()` here
    // TODO: maybe make `Update()` private and the system responsible for calling it a `friend` of the `Camera` struct
    camera = *cameraPtr; // reflect editor changes to the local copy
  }
  camera.Update(); // NOTE: local camera needs to be updated manually
}
bool CameraController::UpdatePosition(float deltaTime) {
  static constexpr auto THRESHOLD = 1e-6f;
  static constexpr auto BOOST_FACTOR_MAX = 10.0f;
  static constexpr auto BOOST_RAMP_UP_TIME = 3.0f;
  static constexpr auto BOOST_RAMP_DOWN_TIME = 1.0f;
  auto input = app.GetWASDKeys();
  // FIXME: prevent movement if there is a key sequence in progress
  if (glm::length2(input) < THRESHOLD)
    input = app.GetArrowKeys();
  if (glm::length2(input) < THRESHOLD)
    return false;
  static auto boostFactor = 1.0f;
  static auto boostTime = .0f;
  auto shift = app.GetKey(GLFWConst::KEY_LEFT_SHIFT);
  if (shift)
    boostTime = std::min(boostTime + deltaTime, BOOST_RAMP_UP_TIME);
  else
    boostTime = std::max(.0f, boostTime - deltaTime * (BOOST_RAMP_UP_TIME / BOOST_RAMP_DOWN_TIME));
  boostFactor = 1.0f + (BOOST_FACTOR_MAX - 1.0f) * (boostTime / BOOST_RAMP_UP_TIME);
  auto velocity = moveSpeed * boostFactor * deltaTime;
  camera.position += (camera.forward * input.y + camera.right * input.x) * velocity;
  return true;
}
bool CameraController::UpdateRotation(glm::vec2 mouseDiff) {
  static constexpr auto THRESHOLD = 1e-6f;
  if (glm::length2(mouseDiff) < THRESHOLD)
    return false;
  auto yaw = glm::angleAxis(-mouseDiff.x, glm::vec3(.0f, 1.f, .0f));
  auto pitch = glm::angleAxis(mouseDiff.y, glm::vec3(1.f, .0f, .0f));
  camera.rotation = yaw * camera.rotation * pitch;
  return true;
}
void CameraController::SetMouselook(bool value) {
  mouselook = value;
}
bool CameraController::GetMouselook() const {
  return mouselook;
}
Camera* CameraController::GetCamera() {
  return &camera;
}
