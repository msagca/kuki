#include <camera_controller.hpp>
#include <component/camera.hpp>
#include <component/component.hpp>
#include <GLFW/glfw3.h>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/trigonometric.hpp>
#include <string>
#include <utility>
#include <variant>
#include <vector>
#include <application.hpp>
#include <component/script.hpp>
using namespace kuki;
void CameraController::Update(float deltaTime) {
  static auto firstEnter = true;
  static glm::vec2 mousePos;
  static glm::vec2 mouseLast;
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
      UpdateRotation(mouseDiff);
  } else
    firstEnter = true;
  if (movementEnabled)
    UpdatePosition(deltaTime);
  camera.UpdateProjection();
  camera.UpdateFrustum();
  auto cameraPtr = app.GetEntityComponent<Camera>(entityId);
  if (cameraPtr) {
    // reflect editor changes to the local copy
    // NOTE: some properties (e.g. position) can only be changed by this script
    camera.type = cameraPtr->type;
    camera.orthoSize = cameraPtr->orthoSize;
    camera.nearPlane = cameraPtr->nearPlane;
    camera.farPlane = cameraPtr->farPlane;
    camera.fov = cameraPtr->fov;
    *cameraPtr = camera; // reflect local changes to the component
  }
}
void CameraController::UpdatePosition(float deltaTime) {
  auto input = app.GetWASDKeys();
  if (input == glm::vec2(.0f))
    input = app.GetArrowKeys();
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
  camera.position += (camera.front * input.y + camera.right * input.x) * velocity;
  camera.UpdateView();
}
void CameraController::UpdateRotation(glm::vec2 mouseDiff) {
  static const auto PITCH_LIMIT = glm::radians(89.9f);
  camera.yaw += mouseDiff.x;
  camera.pitch += mouseDiff.y;
  if (camera.pitch > PITCH_LIMIT)
    camera.pitch = PITCH_LIMIT;
  if (camera.pitch < -PITCH_LIMIT)
    camera.pitch = -PITCH_LIMIT;
  camera.UpdateDirection();
  camera.UpdateView();
}
void CameraController::ToggleMovement(bool value) {
  movementEnabled = value;
}
void CameraController::ToggleRotation(bool value) {
  rotationEnabled = value;
}
const std::string CameraController::GetName() const {
  return "CameraController";
}
std::vector<Property> CameraController::GetProperties() const {
  return {{"MoveSpeed", moveSpeed}, {"MouseSensitivity", mouseSensitivity}};
}
void CameraController::SetProperty(Property property) {
  if (std::holds_alternative<float>(property.value)) {
    auto& value = std::get<float>(property.value);
    if (property.name == "MoveSpeed")
      moveSpeed = value;
    else if (property.name == "MouseSensitivity")
      mouseSensitivity = value;
  }
}
Camera* CameraController::GetCamera() {
  return &camera;
}
