#include <camera_controller.hpp>
#include <cmath>
#include <component/camera.hpp>
#include <component/component.hpp>
#include <GLFW/glfw3.h>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <input_manager.hpp>
CameraController::CameraController(InputManager& inputManager)
  : inputManager(inputManager) {}
void CameraController::UpdateRotation(glm::vec2 mouseDiff) {
  static const auto PITCH_LIMIT = 89.99f;
  camera.yaw += mouseDiff.x;
  camera.pitch += mouseDiff.y;
  if (camera.pitch > PITCH_LIMIT)
    camera.pitch = PITCH_LIMIT;
  if (camera.pitch < -PITCH_LIMIT)
    camera.pitch = -PITCH_LIMIT;
  UpdateDirection();
  UpdateView();
}
void CameraController::UpdatePosition(float deltaTime) {
  auto wasd = inputManager.GetWASD();
  auto doubleSpeed = inputManager.GetKey(GLFW_KEY_LEFT_SHIFT);
  auto velocity = moveSpeed * (doubleSpeed ? 2 : 1) * deltaTime;
  camera.position += (camera.front * wasd.y + camera.right * wasd.x) * velocity;
  UpdateView();
}
void CameraController::UpdateDirection() {
  static const auto WORLD_UP = glm::vec3(.0f, 1.0f, .0f);
  camera.front.x = cos(glm::radians(camera.yaw)) * cos(glm::radians(camera.pitch));
  camera.front.y = sin(glm::radians(camera.pitch));
  camera.front.z = sin(glm::radians(camera.yaw)) * cos(glm::radians(camera.pitch));
  camera.front = glm::normalize(camera.front);
  camera.right = glm::normalize(glm::cross(camera.front, WORLD_UP));
  camera.up = glm::normalize(glm::cross(camera.right, camera.front));
}
void CameraController::UpdateView() {
  camera.view = glm::lookAt(camera.position, camera.position + camera.front, camera.up);
}
void CameraController::UpdateProjection() {
  if (camera.type == CameraType::Perspective)
    camera.projection = glm::perspective(camera.fov, camera.aspect, camera.near, camera.far);
  else {
    auto left = camera.size * camera.aspect;
    auto bottom = camera.size;
    camera.projection = glm::ortho(left, -left, bottom, -bottom, camera.near, camera.far);
  }
}
glm::vec3 CameraController::GetPosition() const {
  return camera.position;
}
Camera& CameraController::GetCamera() {
  return camera;
}
void CameraController::SetCamera(Camera* cameraPtr) {
  this->cameraPtr = cameraPtr;
}
glm::vec3 CameraController::GetFront() const {
  return camera.front;
}
float CameraController::GetFOV() const {
  return camera.fov;
}
float CameraController::GetFar() const {
  return camera.far;
}
glm::mat4 CameraController::GetView() const {
  return camera.view;
}
glm::mat4 CameraController::GetProjection() const {
  return camera.projection;
}
void CameraController::SetAspect(float aspect) {
  camera.aspect = aspect;
  UpdateProjection();
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
    UpdateRotation(mouseDiff);
  } else
    firstEnter = true;
  UpdatePosition(deltaTime);
  if (cameraPtr) {
    // NOTE: by default, all changes are applied to the local copy; they are only reflected to an actual camera component if the scene contains one
    // NOTE: the following components are allowed to be changed through the UI, the rest are updated via the camera controller or window events
    camera.type = cameraPtr->type;
    camera.fov = cameraPtr->fov;
    camera.near = cameraPtr->near;
    camera.far = cameraPtr->far;
    camera.size = cameraPtr->size;
    UpdateProjection();
    *cameraPtr = camera;
  }
}
