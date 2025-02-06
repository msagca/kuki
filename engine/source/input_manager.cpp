#include <GLFW/glfw3.h>
#include <glm/ext/vector_float2.hpp>
#include <input_manager.hpp>
#include <string>
#include <unordered_map>
bool InputManager::GetKey(int key) const {
  auto it = keyStates.find(key);
  if (it == keyStates.end())
    return false;
  return it->second;
}
bool InputManager::GetButton(int button) const {
  auto it = buttonStates.find(button);
  if (it == buttonStates.end())
    return false;
  return it->second;
}
glm::vec2 InputManager::GetWASD() const {
  glm::vec2 wasd(.0f, .0f);
  auto w = GetKey(GLFW_KEY_W);
  auto s = GetKey(GLFW_KEY_S);
  auto a = GetKey(GLFW_KEY_A);
  auto d = GetKey(GLFW_KEY_D);
  wasd.y = w ? (s ? .0f : 1.0f) : (s ? -1.0f : .0f);
  wasd.x = d ? (a ? .0f : 1.0f) : (a ? -1.0f : .0f);
  return wasd;
}
glm::vec2 InputManager::GetArrow() const {
  glm::vec2 arrow(.0f, .0f);
  auto up = GetKey(GLFW_KEY_UP);
  auto down = GetKey(GLFW_KEY_DOWN);
  auto left = GetKey(GLFW_KEY_LEFT);
  auto right = GetKey(GLFW_KEY_RIGHT);
  arrow.y = up ? (down ? .0f : 1.0f) : (down ? -1.0f : .0f);
  arrow.x = right ? (left ? .0f : 1.0f) : (left ? -1.0f : .0f);
  return arrow;
}
glm::vec2 InputManager::GetMousePos() const {
  return mousePos;
}
double InputManager::GetInactivityTime() const {
  return glfwGetTime() - lastInputTime;
}
void InputManager::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
  SetKeyState(key, action);
}
void InputManager::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
  SetButtonState(button, action);
}
void InputManager::CursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
  SetMousePos(xpos, ypos);
}
void InputManager::SetKeyState(int key, int action) {
  lastInputTime = glfwGetTime();
  if (!keysEnabled)
    return;
  // NOTE: key state is either true (press or repeat) or false (release)
  keyStates[key] = action == GLFW_PRESS || action == GLFW_REPEAT;
  if (inactiveCallbacks.find(key) != inactiveCallbacks.end())
    return;
  if (action == GLFW_PRESS) {
    if (pressCallbacks.find(key) != pressCallbacks.end())
      pressCallbacks[key]();
  } else if (action == GLFW_RELEASE)
    if (releaseCallbacks.find(key) != releaseCallbacks.end())
      releaseCallbacks[key]();
}
void InputManager::SetButtonState(int button, int action) {
  lastInputTime = glfwGetTime();
  buttonStates[button] = action == GLFW_PRESS || action == GLFW_REPEAT;
  if (inactiveCallbacks.find(button) != inactiveCallbacks.end())
    return;
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
void InputManager::RegisterCallback(int key, int action, std::function<void()> callback, std::string description) {
  if (action == GLFW_PRESS)
    pressCallbacks[key] = callback;
  else if (action == GLFW_RELEASE)
    releaseCallbacks[key] = callback;
  if (description.empty())
    return;
  keyDescriptions[key] = description;
  updateBindings = true;
}
void InputManager::UnregisterCallback(int key, int action) {
  inactiveCallbacks.erase(key);
  if (action == GLFW_PRESS)
    pressCallbacks.erase(key);
  else if (action == GLFW_RELEASE)
    releaseCallbacks.erase(key);
  keyDescriptions.erase(key);
  updateBindings = true;
}
void InputManager::RegisterKey(int key, std::string description) {
  keyDescriptions[key] = description;
  updateBindings = true;
}
void InputManager::UnregisterKey(int key) {
  keyDescriptions.erase(key);
  updateBindings = true;
}
const std::unordered_map<std::string, std::string>& InputManager::GetKeyBindings() {
  if (updateBindings) {
    updateBindings = false;
    keyBindings.clear();
    for (const auto& pair : keyDescriptions) {
      auto key = GLFWKeyToString(pair.first);
      if (!key.empty())
        keyBindings[key] = pair.second;
    }
  }
  return keyBindings;
}
void InputManager::DisableCallback(int key) {
  if (pressCallbacks.find(key) != pressCallbacks.end())
    inactiveCallbacks.insert(key);
  if (releaseCallbacks.find(key) != releaseCallbacks.end())
    inactiveCallbacks.insert(key);
}
void InputManager::EnableCallback(int key) {
  if (inactiveCallbacks.find(key) != inactiveCallbacks.end())
    inactiveCallbacks.erase(key);
}
void InputManager::DisableKeyCallbacks() {
  keysEnabled = false;
}
void InputManager::EnableKeyCallbacks() {
  keysEnabled = true;
}
std::string InputManager::GLFWKeyToString(int key) {
  switch (key) {
  case GLFW_KEY_A:
    return "A";
  case GLFW_KEY_B:
    return "B";
  case GLFW_KEY_C:
    return "C";
  case GLFW_KEY_D:
    return "D";
  case GLFW_KEY_E:
    return "E";
  case GLFW_KEY_F:
    return "F";
  case GLFW_KEY_G:
    return "G";
  case GLFW_KEY_H:
    return "H";
  case GLFW_KEY_I:
    return "I";
  case GLFW_KEY_J:
    return "J";
  case GLFW_KEY_K:
    return "K";
  case GLFW_KEY_L:
    return "L";
  case GLFW_KEY_M:
    return "M";
  case GLFW_KEY_N:
    return "N";
  case GLFW_KEY_O:
    return "O";
  case GLFW_KEY_P:
    return "P";
  case GLFW_KEY_Q:
    return "Q";
  case GLFW_KEY_R:
    return "R";
  case GLFW_KEY_S:
    return "S";
  case GLFW_KEY_T:
    return "T";
  case GLFW_KEY_U:
    return "U";
  case GLFW_KEY_V:
    return "V";
  case GLFW_KEY_W:
    return "W";
  case GLFW_KEY_X:
    return "X";
  case GLFW_KEY_Y:
    return "Y";
  case GLFW_KEY_Z:
    return "Z";
  case GLFW_KEY_TAB:
    return "TAB";
  case GLFW_KEY_SPACE:
    return "SPACE";
  default:
    return "";
  }
}
