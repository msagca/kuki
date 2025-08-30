#define GLM_ENABLE_EXPERIMENTAL
#include "glm/ext/scalar_constants.hpp"
#include <camera_controller.hpp>
#include <component/camera.hpp>
#include <component/component.hpp>
#include <GLFW/glfw3.h>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/trigonometric.hpp>
#include <application.hpp>
#include <component/script.hpp>
using namespace kuki;
void CameraController::Update(float deltaTime) {
  static auto firstEnter = true;
  static glm::vec2 mousePos;
  static glm::vec2 mouseLast;
  auto posDirty = false;
  auto rotDirty = false;
  if (app.GetButtonDown(GLFW_MOUSE_BUTTON_2)) {
    mousePos = app.GetMousePos();
    if (firstEnter)
      mouseLast = mousePos;
    firstEnter = false;
    glm::vec2 mouseDiff{};
    mouseDiff.x = (mousePos.x - mouseLast.x) * mouseSensitivity;
    mouseDiff.y = (mouseLast.y - mousePos.y) * mouseSensitivity;
    mouseLast = mousePos;
    if (rotationEnabled)
      rotDirty = UpdateRotation(mouseDiff);
  } else
    firstEnter = true;
  posDirty = UpdatePosition(deltaTime);
  auto cameraPtr = app.GetEntityComponent<Camera>(entityId);
  if (cameraPtr) {
    // prioritize controller changes over editor changes
    if (posDirty)
      cameraPtr->position = camera.position;
    if (rotDirty) {
      cameraPtr->pitch = camera.pitch;
      cameraPtr->yaw = camera.yaw;
    }
    // for now, aspect ratio is only allowed to be updated by the rendering system
    cameraPtr->aspectRatio = camera.aspectRatio;
    // reflect editor changes to the local copy
    camera = *cameraPtr;
    cameraPtr->Update();
  }
  camera.Update();
}
bool CameraController::UpdatePosition(float deltaTime) {
  static constexpr auto EPSILON = glm::epsilon<float>();
  static constexpr auto EPSILON2 = EPSILON * EPSILON;
  auto input = app.GetWASDKeys();
  if (glm::length2(input) < EPSILON2)
    input = app.GetArrowKeys();
  if (glm::length2(input) < EPSILON2)
    return false;
  static auto boostFactor = 1.0f;
  static auto boostTime = .0f;
  static const auto maxBoostFactor = 5.0f;
  static const auto boostRampUpTime = 3.0f;
  static const auto boostRampDownTime = 2.0f;
  auto shiftHeld = app.GetKeyDown(GLFW_KEY_LEFT_SHIFT);
  if (shiftHeld)
    boostTime = std::min(boostTime + deltaTime, boostRampUpTime);
  else
    boostTime = std::max(.0f, boostTime - deltaTime * (boostRampUpTime / boostRampDownTime));
  boostFactor = 1.0f + (maxBoostFactor - 1.0f) * (boostTime / boostRampUpTime);
  auto velocity = moveSpeed * boostFactor * deltaTime;
  camera.position += (camera.forward * input.y + camera.right * input.x) * velocity;
  return true;
}
bool CameraController::UpdateRotation(glm::vec2 mouseDiff) {
  static constexpr auto PITCH_LIMIT = glm::radians(89.0f);
  static constexpr auto EPSILON = glm::epsilon<float>();
  static constexpr auto EPSILON2 = EPSILON * EPSILON;
  if (glm::length2(mouseDiff) < EPSILON2)
    return false;
  camera.yaw += mouseDiff.x;
  camera.pitch += mouseDiff.y;
  camera.pitch = std::clamp(camera.pitch, -PITCH_LIMIT, PITCH_LIMIT);
  return true;
}
void CameraController::ToggleRotation(bool value) {
  rotationEnabled = value;
}
Camera* CameraController::GetCamera() {
  return &camera;
}
