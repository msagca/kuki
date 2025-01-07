#include <camera_controller.hpp>
#include <cmath>
#include <component_types.hpp>
#include <GLFW/glfw3.h>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <input_manager.hpp>
extern double deltaTime;
static const auto WORLD_UP = glm::vec3(0.0f, 1.0f, 0.0f);
static const auto PITCH_LIMIT = 89.9f;
static const auto MOVE_SPEED = 2.5f;
static const auto MOUSE_SENSITIVITY = .1f;
void CameraController::UpdateRotation(glm::vec2 mouseDiff) {
  camera.yaw += mouseDiff.x;
  camera.pitch += mouseDiff.y;
  if (camera.pitch > PITCH_LIMIT)
    camera.pitch = PITCH_LIMIT;
  if (camera.pitch < -PITCH_LIMIT)
    camera.pitch = -PITCH_LIMIT;
  UpdateDirection();
  UpdateView();
}
void CameraController::UpdatePosition() {
  auto wasd = inputManager.GetWASD();
  auto doubleSpeed = inputManager.GetKey(GLFW_KEY_LEFT_SHIFT);
  float velocity = moveSpeed * (doubleSpeed ? 2 : 1) * deltaTime;
  camera.position += (camera.front * wasd.y + camera.right * wasd.x) * velocity;
  UpdateView();
}
void CameraController::UpdateDirection() {
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
  camera.projection = glm::perspective(camera.fov, camera.aspect, camera.near, camera.far);
}
CameraController::CameraController(Camera& camera, InputManager& inputManager)
  : camera(camera), inputManager(inputManager), moveSpeed(MOVE_SPEED), mouseSensitivity(MOUSE_SENSITIVITY) {
  UpdateDirection();
  UpdateView();
  UpdateProjection();
}
void CameraController::SetCamera(Camera& camera) {
  this->camera = camera;
}
void CameraController::SetPosition(glm::vec3 position) {
  camera.position = position;
  UpdateView();
}
void CameraController::SetAspect(float aspect) {
  camera.aspect = aspect;
  UpdateProjection();
}
void CameraController::SetFOV(float fov) {
  camera.fov = fov;
  UpdateProjection();
}
void CameraController::Update() {
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
  UpdatePosition();
}
