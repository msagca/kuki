#pragma once
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <cmath>
#include <component_types.hpp>
#include <GLFW/glfw3.h>
#include <input_manager.hpp>
extern double deltaTime;
static const auto WORLD_UP = glm::vec3(0.0f, 1.0f, 0.0f);
static const auto PITCH_LIMIT = 89.9f;
static const auto MOVE_SPEED = 2.5f;
static const auto MOUSE_SENSITIVITY = .1f;
class CameraController {
private:
  Camera& camera;
  InputManager& inputManager;
  float moveSpeed;
  float mouseSensitivity;
  void UpdateRotation(glm::vec2 mouseDiff) {
    camera.yaw += mouseDiff.x;
    camera.pitch += mouseDiff.y;
    if (camera.pitch > PITCH_LIMIT)
      camera.pitch = PITCH_LIMIT;
    if (camera.pitch < -PITCH_LIMIT)
      camera.pitch = -PITCH_LIMIT;
    UpdateDirection();
    UpdateView();
  }
  void UpdatePosition() {
    auto wasd = inputManager.GetWASD();
    auto doubleSpeed = inputManager.GetKey(GLFW_KEY_LEFT_SHIFT);
    float velocity = moveSpeed * (doubleSpeed ? 2 : 1) * deltaTime;
    camera.position += (camera.front * wasd.y + camera.right * wasd.x) * velocity;
    UpdateView();
  }
  void UpdateDirection() {
    camera.front.x = cos(glm::radians(camera.yaw)) * cos(glm::radians(camera.pitch));
    camera.front.y = sin(glm::radians(camera.pitch));
    camera.front.z = sin(glm::radians(camera.yaw)) * cos(glm::radians(camera.pitch));
    camera.front = glm::normalize(camera.front);
    camera.right = glm::normalize(glm::cross(camera.front, WORLD_UP));
    camera.up = glm::normalize(glm::cross(camera.right, camera.front));
  }
  void UpdateView() {
    camera.view = glm::lookAt(camera.position, camera.position + camera.front, camera.up);
  }
  void UpdateProjection() {
    camera.projection = glm::perspective(camera.fov, camera.aspect, camera.near, camera.far);
  }
public:
  CameraController(Camera& camera, InputManager& inputManager)
    : camera(camera), inputManager(inputManager), moveSpeed(MOVE_SPEED), mouseSensitivity(MOUSE_SENSITIVITY) {
    UpdateDirection();
    UpdateView();
    UpdateProjection();
  }
  void SetPosition(glm::vec3 position) {
    camera.position = position;
    UpdateView();
  }
  void SetAspect(float aspect) {
    camera.aspect = aspect;
    UpdateProjection();
  }
  void SetFOV(float fov) {
    camera.fov = fov;
    UpdateProjection();
  }
  void Update() {
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
};
