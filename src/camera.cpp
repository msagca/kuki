#include <camera.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/matrix.hpp>
#include <glm/trigonometric.hpp>
Camera::Camera(glm::vec3 position, glm::vec3 worldUp, float pitch, float yaw, float speed)
  : position(position), worldUp(worldUp), pitch(pitch), yaw(yaw), speed(speed) {
  UpdateVectors();
}
void Camera::ProcessKeyboard(uint8_t wasd, bool shift, float deltaTime) {
  auto velocity = speed * (shift ? 2 : 1) * deltaTime;
  if (wasd & 1) // W
    position += front * velocity;
  if (wasd & 2) // A
    position -= front * velocity;
  if (wasd & 4) // S
    position -= right * velocity;
  if (wasd & 8) // D
    position += right * velocity;
}
void Camera::ProcessMouse(float xOffset, float yOffset) {
  yaw += xOffset;
  pitch += yOffset;
  if (pitch > PITCH_LIMIT)
    pitch = PITCH_LIMIT;
  if (pitch < -PITCH_LIMIT)
    pitch = -PITCH_LIMIT;
  UpdateVectors();
}
void Camera::UpdateVectors() {
  glm::vec3 frontNew{
    cos(glm::radians(yaw)) * cos(glm::radians(pitch)),
    sin(glm::radians(pitch)),
    sin(glm::radians(yaw)) * cos(glm::radians(pitch))};
  front = glm::normalize(frontNew);
  right = glm::normalize(glm::cross(front, worldUp));
  up = glm::normalize(glm::cross(right, front));
}
glm::mat4 Camera::GetViewMatrix() const {
  return glm::lookAt(position, position + front, up);
}
