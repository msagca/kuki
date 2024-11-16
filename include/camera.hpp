#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
const float PITCH = .0f;
const float PITCH_LIMIT = 89.0f;
const float SPEED = 2.5f;
const float YAW = -90.0f;
class Camera {
public:
  Camera(glm::vec3 position = glm::vec3(.0f, .0f, .0f), glm::vec3 worldUp = glm::vec3(.0f, 1.0f, .0f), float pitch = PITCH, float yaw = YAW, float speed = SPEED);
  glm::mat4 GetViewMatrix() const;
  glm::vec3 position;
  void ProcessKeyboard(uint8_t, float);
  void ProcessMouse(float, float);
private:
  float pitch;
  float speed;
  float yaw;
  glm::vec3 front;
  glm::vec3 right;
  glm::vec3 up;
  glm::vec3 worldUp;
  void UpdateVectors();
};
