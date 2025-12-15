#include <GLFW/glfw3.h>
#include <glm/ext/vector_float2.hpp>
#include <input_manager.hpp>
#include <span>
#include <spdlog/spdlog.h>
#include <string>
#include <trie.hpp>
#include <utility>
namespace kuki {
bool InputManager::GetState(int input) const {
  auto index = GLFWInputToIndex(input);
  return inputState[index];
}
bool InputManager::IsPressed(int input) const {
  auto index = GLFWInputToIndex(input);
  return pressState[index];
}
bool InputManager::IsReleased(int input) const {
  auto index = GLFWInputToIndex(input);
  return releaseState[index];
}
glm::vec2 InputManager::GetWASD() const {
  glm::vec2 wasd(0.f, 0.f);
  auto w = GetState(GLFW_KEY_W);
  auto s = GetState(GLFW_KEY_S);
  auto a = GetState(GLFW_KEY_A);
  auto d = GetState(GLFW_KEY_D);
  wasd.y = w ? (s ? 0.f : 1.f) : (s ? -1.f : 0.f);
  wasd.x = d ? (a ? 0.f : 1.f) : (a ? -1.f : 0.f);
  return wasd;
}
glm::vec2 InputManager::GetArrow() const {
  glm::vec2 arrow(0.f, 0.f);
  auto up = GetState(GLFW_KEY_UP);
  auto down = GetState(GLFW_KEY_DOWN);
  auto left = GetState(GLFW_KEY_LEFT);
  auto right = GetState(GLFW_KEY_RIGHT);
  arrow.y = up ? (down ? 0.f : 1.f) : (down ? -1.f : 0.f);
  arrow.x = right ? (left ? 0.f : 1.f) : (left ? -1.f : 0.f);
  return arrow;
}
glm::vec2 InputManager::GetMousePos() const {
  return mousePos;
}
double InputManager::GetInactivityTime() const {
  return glfwGetTime() - lastInputTime;
}
static inline bool IsASCII(unsigned int codepoint) {
  return codepoint <= 0x7F;
}
static inline bool IsASCIIPrintable(unsigned int codepoint) {
  return codepoint >= 0x20 && codepoint <= 0x7E;
}
static inline unsigned char ToASCII(unsigned int codepoint) {
  return static_cast<unsigned char>(codepoint);
}
void InputManager::CharCallback(GLFWwindow* window, unsigned int codepoint) {
  if (!keysEnabled)
    return;
  if (!IsASCIIPrintable(codepoint))
    return;
  auto c = ToASCII(codepoint);
  keyseq.push_back(c);
  // NOTE: if there is an action registered to this sequence, it will fire during the trie traversal
  auto found = keymap.FindWord(keyseq.begin(), keyseq.end());
  if (found)
    keyseq.clear();
  else {
    // TODO: this call could be avoided if FindWord returned an enum (NotFound, PrefixFound, ExactFound)
    found = keymap.FindPrefix(keyseq.begin(), keyseq.end());
    if (!found)
      keyseq.clear();
  }
}
bool InputManager::SequenceInProgress() const {
  return !keyseq.empty();
}
void InputManager::FireAction(unsigned char index) {
  if (!keysEnabled || SequenceInProgress())
    return;
  if (pressState[index]) {
    if (auto it = pressActions.find(index); it != pressActions.end())
      it->second();
  } else if (releaseState[index])
    if (auto it = releaseActions.find(index); it != releaseActions.end())
      it->second();
}
void InputManager::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
  if (!keysEnabled)
    return;
  lastInputTime = glfwGetTime();
  static constexpr int cancelKeys[] = {GLFW_KEY_ESCAPE, GLFW_KEY_BACKSPACE, GLFW_KEY_DELETE, GLFW_KEY_SPACE};
  for (const auto& k : std::span(cancelKeys))
    if (key == k)
      keyseq.clear();
  auto index = GLFWInputToIndex(key);
  inputState[index] = action == GLFW_PRESS || action == GLFW_REPEAT;
  pressState[index] = action == GLFW_PRESS;
  releaseState[index] = action == GLFW_RELEASE;
  FireAction(index);
}
void InputManager::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
  if (!buttonsEnabled)
    return;
  lastInputTime = glfwGetTime();
  auto index = GLFWInputToIndex(button);
  pressState[index] = action == GLFW_PRESS;
  releaseState[index] = action == GLFW_RELEASE;
  FireAction(index);
}
void InputManager::CursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
  if (!buttonsEnabled)
    return;
  lastInputTime = glfwGetTime();
  mousePos.x = xpos;
  mousePos.y = ypos;
}
bool InputManager::RegisterAction(const std::string& trigger, InputAction action) {
  auto result = keymap.Insert(trigger, action);
  if (!result)
    spdlog::warn("Failed to register action for trigger '{}'. It may be a prefix of another trigger or contain a subsequence that is already registered.", trigger);
  else
    spdlog::info("Registered action for trigger '{}'.", trigger);
  return result;
}
void InputManager::RegisterAction(int trigger, InputAction action, bool press) {
  // FIXME: if the trigger is ASCII printable, register it as a key sequence instead; otherwise, multiple actions may be registered for the same key
  auto index = GLFWInputToIndex(trigger);
  auto triggerStr = GLFWKeyToString(trigger);
  if (press) {
    auto exists = pressActions.find(index) != pressActions.end();
    if (exists)
      spdlog::warn("Overwriting existing press action for key/button '{}'.", triggerStr);
    else
      spdlog::info("Registered press action for key/button '{}'.", triggerStr);
    pressActions[index] = std::move(action);
  } else {
    auto exists = releaseActions.find(index) != releaseActions.end();
    if (exists)
      spdlog::warn("Overwriting existing release action for key/button '{}'.", triggerStr);
    else
      spdlog::info("Registered release action for key/button '{}'.", triggerStr);
    releaseActions[index] = std::move(action);
  }
}
bool InputManager::UnregisterAction(const std::string& trigger) {
  auto result = keymap.Remove(trigger);
  if (result)
    spdlog::info("Unregistered action for trigger '{}'.", trigger);
  else
    spdlog::warn("No action found for trigger '{}'.", trigger);
  return result;
}
bool InputManager::UnregisterAction(int trigger, bool press) {
  auto index = GLFWInputToIndex(trigger);
  auto triggerStr = GLFWKeyToString(trigger);
  bool result{false};
  if (press) {
    result = pressActions.erase(index);
    if (!result)
      spdlog::warn("No press action found for key/button '{}'.", triggerStr);
    return result;
  }
  result = releaseActions.erase(index);
  if (!result)
    spdlog::warn("No release action found for key/button '{}'.", triggerStr);
  return result;
}
void InputManager::EnableKeys() {
  keysEnabled = true;
  spdlog::info("Keys are enabled.");
}
void InputManager::DisableKeys() {
  keysEnabled = false;
  spdlog::info("Keys are disabled.");
}
void InputManager::EnableButtons() {
  buttonsEnabled = true;
  spdlog::info("Buttons are enabled.");
}
void InputManager::DisableButtons() {
  buttonsEnabled = false;
  spdlog::info("Buttons are disabled.");
}
void InputManager::EnableAll() {
  keysEnabled = true;
  buttonsEnabled = true;
  spdlog::info("Inputs are enabled.");
}
void InputManager::DisableAll() {
  keysEnabled = false;
  buttonsEnabled = false;
  keyseq.clear();
  spdlog::info("Inputs are disabled.");
}
unsigned char InputManager::GLFWInputToIndex(int key) {
  if (key >= GLFW_KEY_SPACE && key <= GLFW_KEY_GRAVE_ACCENT)
    return static_cast<unsigned char>(key - GLFW_KEY_SPACE);
  if (key >= GLFW_KEY_F1 && key <= GLFW_KEY_F12)
    return static_cast<unsigned char>(65 + key - GLFW_KEY_F1);
  switch (key) {
  case GLFW_KEY_ESCAPE:
    return 77;
  case GLFW_KEY_ENTER:
    return 78;
  case GLFW_KEY_TAB:
    return 79;
  case GLFW_KEY_BACKSPACE:
    return 80;
  case GLFW_KEY_INSERT:
    return 81;
  case GLFW_KEY_DELETE:
    return 82;
  case GLFW_KEY_RIGHT:
    return 83;
  case GLFW_KEY_LEFT:
    return 84;
  case GLFW_KEY_DOWN:
    return 85;
  case GLFW_KEY_UP:
    return 86;
  case GLFW_KEY_PAGE_UP:
    return 87;
  case GLFW_KEY_PAGE_DOWN:
    return 88;
  case GLFW_KEY_HOME:
    return 89;
  case GLFW_KEY_END:
    return 90;
  case GLFW_KEY_CAPS_LOCK:
    return 91;
  case GLFW_KEY_SCROLL_LOCK:
    return 92;
  case GLFW_KEY_NUM_LOCK:
    return 93;
  case GLFW_KEY_PRINT_SCREEN:
    return 94;
  case GLFW_KEY_PAUSE:
    return 95;
  }
  switch (key) {
  case GLFW_KEY_LEFT_SHIFT:
    return 96;
  case GLFW_KEY_LEFT_CONTROL:
    return 97;
  case GLFW_KEY_LEFT_ALT:
    return 98;
  case GLFW_KEY_LEFT_SUPER:
    return 99;
  case GLFW_KEY_RIGHT_SHIFT:
    return 100;
  case GLFW_KEY_RIGHT_CONTROL:
    return 101;
  case GLFW_KEY_RIGHT_ALT:
    return 102;
  case GLFW_KEY_RIGHT_SUPER:
    return 103;
  }
  if (key >= GLFW_KEY_KP_0 && key <= GLFW_KEY_KP_9)
    return static_cast<unsigned char>(104 + key - GLFW_KEY_KP_0);
  switch (key) {
  case GLFW_KEY_KP_DECIMAL:
    return 114;
  case GLFW_KEY_KP_DIVIDE:
    return 115;
  case GLFW_KEY_KP_MULTIPLY:
    return 116;
  case GLFW_KEY_KP_SUBTRACT:
    return 117;
  case GLFW_KEY_KP_ADD:
    return 118;
  case GLFW_KEY_KP_ENTER:
    return 119;
  case GLFW_KEY_KP_EQUAL:
    return 120;
  }
  if (key == GLFW_KEY_MENU)
    return 121;
  if (key >= GLFW_MOUSE_BUTTON_1 && key <= GLFW_MOUSE_BUTTON_8)
    return static_cast<unsigned char>(122 + key - GLFW_MOUSE_BUTTON_1);
  return 255;
}
std::string InputManager::GLFWKeyToString(int key) {
  if (key >= GLFW_KEY_SPACE && key <= GLFW_KEY_GRAVE_ACCENT)
    return std::string(1, static_cast<char>(key));
  if (key >= GLFW_KEY_F1 && key <= GLFW_KEY_F12)
    return "F" + std::to_string(1 + key - GLFW_KEY_F1);
  switch (key) {
  case GLFW_KEY_ESCAPE:
    return "Escape";
  case GLFW_KEY_ENTER:
    return "Enter";
  case GLFW_KEY_TAB:
    return "Tab";
  case GLFW_KEY_BACKSPACE:
    return "Backspace";
  case GLFW_KEY_INSERT:
    return "Insert";
  case GLFW_KEY_DELETE:
    return "Delete";
  case GLFW_KEY_RIGHT:
    return "Right";
  case GLFW_KEY_LEFT:
    return "Left";
  case GLFW_KEY_DOWN:
    return "Down";
  case GLFW_KEY_UP:
    return "Up";
  case GLFW_KEY_PAGE_UP:
    return "PageUp";
  case GLFW_KEY_PAGE_DOWN:
    return "PageDown";
  case GLFW_KEY_HOME:
    return "Home";
  case GLFW_KEY_END:
    return "End";
  case GLFW_KEY_CAPS_LOCK:
    return "CapsLock";
  case GLFW_KEY_SCROLL_LOCK:
    return "ScrollLock";
  case GLFW_KEY_NUM_LOCK:
    return "NumLock";
  case GLFW_KEY_PRINT_SCREEN:
    return "PrintScreen";
  case GLFW_KEY_PAUSE:
    return "Pause";
  }
  switch (key) {
  case GLFW_KEY_LEFT_SHIFT:
    return "LeftShift";
  case GLFW_KEY_LEFT_CONTROL:
    return "LeftControl";
  case GLFW_KEY_LEFT_ALT:
    return "LeftAlt";
  case GLFW_KEY_LEFT_SUPER:
    return "LeftSuper";
  case GLFW_KEY_RIGHT_SHIFT:
    return "RightShift";
  case GLFW_KEY_RIGHT_CONTROL:
    return "RightControl";
  case GLFW_KEY_RIGHT_ALT:
    return "RightAlt";
  case GLFW_KEY_RIGHT_SUPER:
    return "RightSuper";
  }
  if (key >= GLFW_KEY_KP_0 && key <= GLFW_KEY_KP_9)
    return "Keypad" + std::to_string(key - GLFW_KEY_KP_0);
  switch (key) {
  case GLFW_KEY_KP_DECIMAL:
    return "KeypadDecimal";
  case GLFW_KEY_KP_DIVIDE:
    return "KeypadDivide";
  case GLFW_KEY_KP_MULTIPLY:
    return "KeypadMultiply";
  case GLFW_KEY_KP_SUBTRACT:
    return "KeypadSubtract";
  case GLFW_KEY_KP_ADD:
    return "KeypadAdd";
  case GLFW_KEY_KP_ENTER:
    return "KeypadEnter";
  case GLFW_KEY_KP_EQUAL:
    return "KeypadEqual";
  }
  if (key == GLFW_KEY_MENU)
    return "Menu";
  if (key >= GLFW_MOUSE_BUTTON_1 && key <= GLFW_MOUSE_BUTTON_8)
    return "MouseButton" + std::to_string(1 + key - GLFW_MOUSE_BUTTON_1);
  return "Unknown";
}
} // namespace kuki
