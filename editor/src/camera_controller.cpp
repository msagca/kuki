#include <camera_controller.hpp>
#include <component/camera.hpp>
#include <component/component.hpp>
#include <GLFW/glfw3.h>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <input_manager.hpp>
CameraController::CameraController(InputManager& inputManager)
  : inputManager(inputManager) {}
void CameraController::UpdatePosition(float deltaTime) {
  auto input = inputManager.GetWASD();
  if (input == glm::vec2(.0f))
    input = inputManager.GetArrow();
  static auto boostFactor = 1.0f;
  static auto boostTime = .0f;
  static const auto maxBoostFactor = 5.0f;
  static const auto boostRampUpTime = 3.0f;
  static const auto boostRampDownTime = 2.0f;
  auto shiftHeld = inputManager.GetKey(GLFW_KEY_LEFT_SHIFT);
  if (shiftHeld)
    boostTime = std::min(boostTime + deltaTime, boostRampUpTime);
  else
    boostTime = std::max(.0f, boostTime - deltaTime * (boostRampUpTime / boostRampDownTime));
  boostFactor = 1.0f + (maxBoostFactor - 1.0f) * (boostTime / boostRampUpTime);
  auto velocity = moveSpeed * boostFactor * deltaTime;
  camera.position += (camera.front * input.y + camera.right * input.x) * velocity;
  camera.UpdateView();
}
void CameraController::UpdateRotation(glm::vec2 mouseDiff) {
  static const auto PITCH_LIMIT = 89.99f;
  camera.yaw += mouseDiff.x;
  camera.pitch += mouseDiff.y;
  if (camera.pitch > PITCH_LIMIT)
    camera.pitch = PITCH_LIMIT;
  if (camera.pitch < -PITCH_LIMIT)
    camera.pitch = -PITCH_LIMIT;
  camera.UpdateDirection();
  camera.UpdateView();
}
void CameraController::SetCamera(Camera* cameraPtr) {
  this->cameraPtr = cameraPtr;
}
Camera* CameraController::GetCamera() {
  return cameraPtr;
}
void CameraController::Update(float deltaTime) {
  static auto firstEnter = true;
  static glm::vec2 mousePos;
  static glm::vec2 mouseLast;
  if (inputManager.GetButton(GLFW_MOUSE_BUTTON_2)) {
    mousePos = inputManager.GetMousePos();
    if (firstEnter)
      mouseLast = mousePos;
    firstEnter = false;
    glm::vec2 mouseDiff{};
    mouseDiff.x = (mousePos.x - mouseLast.x) * mouseSensitivity;
    mouseDiff.y = (mouseLast.y - mousePos.y) * mouseSensitivity;
    mouseLast = mousePos;
    if (rotationEnabled)
      UpdateRotation(mouseDiff);
  } else
    firstEnter = true;
  if (movementEnabled)
    UpdatePosition(deltaTime);
  camera.UpdateProjection();
  if (cameraPtr) { // NOTE: by default, all changes are applied to the local copy; they are only reflected to an actual camera component if the scene contains one
    camera.type = cameraPtr->type; // reflect editor changes to the local copy
    *cameraPtr = camera;
  }
}
void CameraController::ToggleMovement(bool value) {
  movementEnabled = value;
}
void CameraController::ToggleRotation(bool value) {
  rotationEnabled = value;
}
