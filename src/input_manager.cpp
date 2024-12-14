#include <GLFW/glfw3.h>
#include <input_manager.hpp>
extern double deltaTime;
enum KeyBit {
  BitEscape,
  BitEnter,
  BitSpace,
  BitBackspace,
  BitLeftShift,
  BitRightShift,
  BitW,
  BitS,
  BitA,
  BitD,
  BitUp,
  BitDown,
  BitLeft,
  BitRight,
  BitH,
  BitP
};
enum KeyMask {
  MaskEscape = 1 << BitEscape,
  MaskEnter = 1 << BitEnter,
  MaskSpace = 1 << BitSpace,
  MaskBackspace = 1 << BitBackspace,
  MaskLeftShift = 1 << BitLeftShift,
  MaskRightShift = 1 << BitRightShift,
  MaskW = 1 << BitW,
  MaskS = 1 << BitS,
  MaskA = 1 << BitA,
  MaskD = 1 << BitD,
  MaskUp = 1 << BitUp,
  MaskDown = 1 << BitDown,
  MaskLeft = 1 << BitLeft,
  MaskRight = 1 << BitRight,
  MaskH = 1 << BitH,
  MaskP = 1 << BitP
};
void InputManager::Initialize(GLFWwindow* window) {
  this->window = window;
  glfwSetKeyCallback(window, KeyCallback);
  glfwSetMouseButtonCallback(window, MouseButtonCallback);
  glfwSetCursorPosCallback(window, CursorPosCallback);
}
bool InputManager::GetKey(int key) {
  return keyMask & KeyToBitmask(key);
}
bool InputManager::GetKey(int key1, int key2) {
  return keyMask & (KeyToBitmask(key1) | KeyToBitmask(key2));
}
bool InputManager::GetButton(int button) {
  return glfwGetMouseButton(window, button) == GLFW_PRESS;
}
glm::vec2 InputManager::GetWASD() const {
  glm::vec2 wasd(.0f, .0f);
  auto forward = (keyMask & MaskW) ? ((keyMask & MaskS) ? .0f : 1.0f) : ((keyMask & MaskS) ? -1.0f : .0f);
  auto right = (keyMask & MaskD) ? ((keyMask & MaskA) ? .0f : 1.0f) : ((keyMask & MaskA) ? -1.0f : .0f);
  wasd.y = forward;
  wasd.x = right;
  return wasd;
}
glm::vec2 InputManager::GetArrow() const {
  glm::vec2 arrow(.0f, .0f);
  auto forward = (keyMask & MaskUp) ? ((keyMask & MaskDown) ? .0f : 1.0f) : ((keyMask & MaskDown) ? -1.0f : .0f);
  auto right = (keyMask & MaskRight) ? ((keyMask & MaskLeft) ? .0f : 1.0f) : ((keyMask & MaskLeft) ? -1.0f : .0f);
  arrow.y = forward;
  arrow.x = right;
  return arrow;
}
glm::vec2 InputManager::GetMousePos() const {
  return mousePos;
}
double InputManager::GetInactivityTime() const {
  return glfwGetTime() - lastInputTime;
}
void InputManager::SetKeyState(int key, int action) {
  lastInputTime = glfwGetTime();
  if (action == GLFW_PRESS) {
    keyMask |= KeyToBitmask(key);
    if (pressCallbacks.find(key) != pressCallbacks.end())
      pressCallbacks[key]();
  } else if (action == GLFW_RELEASE) {
    keyMask &= ~KeyToBitmask(key);
    if (releaseCallbacks.find(key) != releaseCallbacks.end())
      releaseCallbacks[key]();
  } else if (action == GLFW_REPEAT) {
    keyMask |= KeyToBitmask(key);
    if (repeatCallbacks.find(key) != repeatCallbacks.end())
      repeatCallbacks[key]();
  }
}
void InputManager::SetButtonState(int button, int action) {
  lastInputTime = glfwGetTime();
  if (action == GLFW_PRESS) {
    if (pressCallbacks.find(button) != pressCallbacks.end())
      pressCallbacks[button]();
  } else if (action == GLFW_RELEASE) {
    if (releaseCallbacks.find(button) != releaseCallbacks.end())
      releaseCallbacks[button]();
  }
}
void InputManager::SetMousePos(double xpos, double ypos) {
  lastInputTime = glfwGetTime();
  mousePos.x = xpos;
  mousePos.y = ypos;
}
void InputManager::RegisterKeyCallback(int key, int action, std::function<void()> callback) {
  if (action == GLFW_PRESS)
    pressCallbacks[key] = callback;
  else if (action == GLFW_RELEASE)
    releaseCallbacks[key] = callback;
  else if (action == GLFW_REPEAT)
    repeatCallbacks[key] = callback;
}
void InputManager::RegisterMouseCallback(int button, int action, std::function<void()> callback) {
  if (action == GLFW_PRESS)
    pressCallbacks[button] = callback;
  else if (action == GLFW_RELEASE)
    releaseCallbacks[button] = callback;
}
uint32_t InputManager::KeyToBitmask(int key) {
  switch (key) {
  case GLFW_KEY_ESCAPE:
    return MaskEscape;
  case GLFW_KEY_ENTER:
    return MaskEnter;
  case GLFW_KEY_SPACE:
    return MaskSpace;
  case GLFW_KEY_BACKSPACE:
    return MaskBackspace;
  case GLFW_KEY_LEFT_SHIFT:
    return MaskLeftShift;
  case GLFW_KEY_RIGHT_SHIFT:
    return MaskRightShift;
  case GLFW_KEY_W:
    return MaskW;
  case GLFW_KEY_A:
    return MaskA;
  case GLFW_KEY_S:
    return MaskS;
  case GLFW_KEY_D:
    return MaskD;
  case GLFW_KEY_UP:
    return MaskUp;
  case GLFW_KEY_DOWN:
    return MaskDown;
  case GLFW_KEY_LEFT:
    return MaskLeft;
  case GLFW_KEY_RIGHT:
    return MaskRight;
  case GLFW_KEY_H:
    return MaskH;
  case GLFW_KEY_P:
    return MaskP;
  default:
    return 0;
  }
}
