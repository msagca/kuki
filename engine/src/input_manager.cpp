#include <GLFW/glfw3.h>
#include <cctype>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_int2.hpp>
#include <input_manager.hpp>
#include <span>
#include <spdlog/spdlog.h>
#include <string>
#include <trie.hpp>
#include <utility>
namespace kuki {
static inline auto IsASCII(unsigned int codepoint) -> bool {
  return codepoint <= 0x7F;
}
static inline auto IsASCIIPrintable(unsigned int codepoint) -> bool {
  return codepoint >= 0x20 && codepoint <= 0x7E;
}
static inline auto ToASCII(unsigned int codepoint) -> unsigned char {
  return static_cast<unsigned char>(codepoint);
}
static inline auto ToLowerASCII(unsigned char c) -> unsigned char {
  return static_cast<unsigned char>(std::tolower(c));
}
static inline auto IsModifierKey(int key) -> bool {
  switch (key) {
  case GLFW_KEY_LEFT_SHIFT:
  case GLFW_KEY_RIGHT_SHIFT:
  case GLFW_KEY_LEFT_CONTROL:
  case GLFW_KEY_RIGHT_CONTROL:
  case GLFW_KEY_LEFT_ALT:
  case GLFW_KEY_RIGHT_ALT:
  case GLFW_KEY_LEFT_SUPER:
  case GLFW_KEY_RIGHT_SUPER:
    return true;
  default:
    return false;
  }
}
InputManager::InputManager(Application &app)
  : Manager(app) {}
auto InputManager::BeginKeyCapture() -> void {
  capturing = true;
  captureResolved = false;
}
auto InputManager::CancelKeyCapture() -> void {
  capturing = false;
  captureResolved = false;
}
auto InputManager::CharCallback(GLFWwindow *window, unsigned int codepoint) -> void {
  if (!keysEnabled)
    return;
  if (!IsASCIIPrintable(codepoint))
    return;
  const auto c = ToLowerASCII(ToASCII(codepoint));
  keyseq.push_back(c);
  // NOTE: if there is an action registered to this sequence, it will fire during the trie traversal
  if (keymap.Find(keyseq.begin(), keyseq.end()) != TrieMatch::PrefixFound)
    keyseq.clear();
}
auto InputManager::CursorPosCallback(GLFWwindow *window, double xpos, double ypos) -> void {
  if (!buttonsEnabled)
    return;
  lastInputTime = glfwGetTime();
  mousePosition.x = xpos;
  mousePosition.y = ypos;
}
auto InputManager::DisableAll() -> void {
  keysEnabled = false;
  buttonsEnabled = false;
  keyseq.clear();
}
auto InputManager::DisableButtons() -> void {
  buttonsEnabled = false;
}
auto InputManager::DisableKeys() -> void {
  keysEnabled = false;
}
auto InputManager::EnableAll() -> void {
  keysEnabled = true;
  buttonsEnabled = true;
}
auto InputManager::EnableButtons() -> void {
  buttonsEnabled = true;
}
auto InputManager::EnableKeys() -> void {
  keysEnabled = true;
}
auto InputManager::GetArrowKeys() const -> glm::ivec2 {
  glm::ivec2 arrow{};
  const auto up = GetState(GLFW_KEY_UP);
  const auto down = GetState(GLFW_KEY_DOWN);
  const auto left = GetState(GLFW_KEY_LEFT);
  const auto right = GetState(GLFW_KEY_RIGHT);
  arrow.y = up ? (down ? 0 : 1) : (down ? -1 : 0);
  arrow.x = right ? (left ? 0 : 1) : (left ? -1 : 0);
  return arrow;
}
auto InputManager::GetBinding(const std::string &name) const -> Trigger {
  auto it = nameToBinding.find(name);
  return it != nameToBinding.end() ? it->second : Trigger{};
}
auto InputManager::GetBindingDescription(const std::string &name) const -> std::string {
  auto it = nameToDescription.find(name);
  return it != nameToDescription.end() ? it->second : "";
}
auto InputManager::GetBindingNames() const -> const std::vector<std::string> & {
  return bindingOrder;
}
auto InputManager::GetInactivityTime() const -> double {
  return glfwGetTime() - lastInputTime;
}
auto InputManager::GetMousePosition() const -> glm::vec2 {
  return mousePosition;
}
auto InputManager::GetScrollOffset() const -> glm::vec2 {
  return scrollOffset;
}
auto InputManager::GetState(int input) const -> bool {
  const auto index = GLFWInputToIndex(input);
  return inputState[index];
}
auto InputManager::GetTriggerName(const Trigger &trigger) const -> std::string {
  if (trigger.key == 0)
    return "";
  std::string name;
  if (trigger.mods & GLFW_MOD_CONTROL)
    name += "Ctrl+";
  if (trigger.mods & GLFW_MOD_ALT)
    name += "Alt+";
  if (trigger.mods & GLFW_MOD_SHIFT)
    name += "Shift+";
  if (trigger.mods & GLFW_MOD_SUPER)
    name += "Super+";
  name += GLFWKeyToString(trigger.key);
  return name;
}
auto InputManager::GetWASD() const -> glm::ivec2 {
  glm::ivec2 wasd{};
  const auto w = GetState(GLFW_KEY_W);
  const auto s = GetState(GLFW_KEY_S);
  const auto a = GetState(GLFW_KEY_A);
  const auto d = GetState(GLFW_KEY_D);
  wasd.y = w ? (s ? 0 : 1) : (s ? -1 : 0);
  wasd.x = d ? (a ? 0 : 1) : (a ? -1 : 0);
  return wasd;
}
auto InputManager::IsBindingHeld(const std::string &name) const -> bool {
  auto it = nameToBinding.find(name);
  if (it == nameToBinding.end())
    return false;
  const auto index = GLFWInputToIndex(it->second.key);
  if (index == 255)
    return false;
  return inputState[index] && (CurrentMods() & it->second.mods) == it->second.mods;
}
auto InputManager::IsBindingPressed(const std::string &name) const -> bool {
  auto it = nameToBinding.find(name);
  if (it == nameToBinding.end())
    return false;
  const auto index = GLFWInputToIndex(it->second.key);
  if (index == 255)
    return false;
  return pressState[index] && (CurrentMods() & it->second.mods) == it->second.mods;
}
auto InputManager::IsPressed(int input) const -> bool {
  const auto index = GLFWInputToIndex(input);
  return pressState[index];
}
auto InputManager::IsReleased(int input) const -> bool {
  const auto index = GLFWInputToIndex(input);
  return releaseState[index];
}
auto InputManager::KeyCallback(GLFWwindow *window, int key, int scancode, int action, int mods) -> void {
  const auto index = GLFWInputToIndex(key);
  if (capturing) {
    lastInputTime = glfwGetTime();
    if (index != 255)
      inputState[index] = action == GLFW_PRESS || action == GLFW_REPEAT;
    if (action == GLFW_PRESS && !IsModifierKey(key)) {
      capturing = false;
      captureResolved = true;
      capturedResult = key == GLFW_KEY_ESCAPE ? CaptureOutcome{CaptureState::Cancelled, {}} : CaptureOutcome{CaptureState::Captured, Trigger{key, CurrentMods()}};
    }
    return;
  }
  if (!keysEnabled)
    return;
  lastInputTime = glfwGetTime();
  static constexpr int cancelKeys[] = {GLFW_KEY_ESCAPE, GLFW_KEY_BACKSPACE, GLFW_KEY_DELETE, GLFW_KEY_SPACE};
  for (const auto &k : std::span(cancelKeys))
    if (key == k)
      keyseq.clear();
  inputState[index] = action == GLFW_PRESS || action == GLFW_REPEAT;
  pressState[index] = pressState[index] || action == GLFW_PRESS;
  releaseState[index] = releaseState[index] || action == GLFW_RELEASE;
  FireAction(index);
}
auto InputManager::MouseButtonCallback(GLFWwindow *window, int button, int action, int mods) -> void {
  if (!buttonsEnabled)
    return;
  lastInputTime = glfwGetTime();
  const auto index = GLFWInputToIndex(button);
  pressState[index] = pressState[index] || action == GLFW_PRESS;
  releaseState[index] = releaseState[index] || action == GLFW_RELEASE;
  FireAction(index);
}
auto InputManager::PollKeyCapture() -> CaptureOutcome {
  if (!captureResolved)
    return {CaptureState::Pending, {}};
  captureResolved = false;
  return capturedResult;
}
auto InputManager::RegisterAction(int trigger, InputAction action, bool press) -> ActionID {
  const auto index = GLFWInputToIndex(trigger);
  if (index == 255) {
    spdlog::warn("[InputManager] failed to register action: invalid key/button");
    return InvalidActionID;
  }
  if (press && trigger >= GLFW_KEY_SPACE && trigger <= GLFW_KEY_GRAVE_ACCENT) {
    return RegisterSequence(GLFWKeyToString(trigger), std::move(action));
  }
  const auto id = nextActionId++;
  Registration registration;
  registration.kind = press ? Registration::Kind::Press : Registration::Kind::Release;
  registration.mapIt = press ? pressActions.emplace(index, std::move(action)) : releaseActions.emplace(index, std::move(action));
  idToRegistration.emplace(id, std::move(registration));
  spdlog::info("[InputManager] registered {} action for key/button: {}", press ? "press" : "release", GLFWKeyToString(trigger));
  return id;
}
auto InputManager::RegisterBinding(const std::string &name, const Trigger &trigger, std::string description) -> void {
  if (nameToBinding.contains(name))
    return;
  bindingOrder.push_back(name);
  nameToDescription[name] = std::move(description);
  AssignBinding(name, trigger);
}
auto InputManager::SetBinding(const std::string &name, const Trigger &trigger) -> void {
  AssignBinding(name, trigger);
}
auto InputManager::AssignBinding(const std::string &name, const Trigger &trigger) -> void {
  if (trigger.key != 0)
    for (auto &[otherName, otherTrigger] : nameToBinding)
      if (otherName != name && otherTrigger == trigger)
        otherTrigger = Trigger{};
  nameToBinding[name] = trigger;
}
auto InputManager::UnregisterAction(const ActionID id) -> bool {
  auto it = idToRegistration.find(id);
  if (it == idToRegistration.end())
    return false;
  switch (it->second.kind) {
  case Registration::Kind::Press:
    pressActions.erase(it->second.mapIt);
    break;
  case Registration::Kind::Release:
    releaseActions.erase(it->second.mapIt);
    break;
  case Registration::Kind::Sequence:
    keymap.Remove(it->second.sequence);
    sequenceToId.erase(it->second.sequence);
    sequenceToDescription.erase(it->second.sequence);
    std::erase(sequenceOrder, it->second.sequence);
    break;
  }
  idToRegistration.erase(it);
  return true;
}
auto InputManager::ResetPulses() -> void {
  pressState.reset();
  releaseState.reset();
}
auto InputManager::ResetScroll() -> void {
  scrollOffset = {};
}
auto InputManager::ScrollCallback(GLFWwindow *window, double xoffset, double yoffset) -> void {
  if (!buttonsEnabled)
    return;
  lastInputTime = glfwGetTime();
  scrollOffset.x += static_cast<float>(xoffset);
  scrollOffset.y += static_cast<float>(yoffset);
}
auto InputManager::RegisterAction(const std::string &trigger, InputAction action, std::string description) -> ActionID {
  auto sequence = trigger;
  for (auto &c : sequence)
    c = static_cast<char>(ToLowerASCII(static_cast<unsigned char>(c)));
  return RegisterSequence(std::move(sequence), std::move(action), std::move(description));
}
auto InputManager::RegisterSequence(std::string sequence, InputAction action, std::string description) -> ActionID {
  if (auto it = sequenceToId.find(sequence); it != sequenceToId.end())
    UnregisterAction(it->second);
  if (!keymap.Insert(sequence, std::move(action))) {
    spdlog::warn("[InputManager] failed to register action for trigger: {}", sequence);
    return InvalidActionID;
  }
  spdlog::info("[InputManager] registered action for trigger: {}", sequence);
  const auto id = nextActionId++;
  Registration registration;
  registration.kind = Registration::Kind::Sequence;
  registration.sequence = sequence;
  idToRegistration.emplace(id, std::move(registration));
  sequenceToId[sequence] = id;
  sequenceOrder.push_back(sequence);
  sequenceToDescription[sequence] = std::move(description);
  return id;
}
auto InputManager::GetSequenceDescription(const std::string &sequence) const -> std::string {
  auto it = sequenceToDescription.find(sequence);
  return it != sequenceToDescription.end() ? it->second : "";
}
auto InputManager::GetSequenceNames() const -> const std::vector<std::string> & {
  return sequenceOrder;
}
auto InputManager::CurrentMods() const -> int {
  auto mods = 0;
  if (inputState[GLFWInputToIndex(GLFW_KEY_LEFT_SHIFT)] || inputState[GLFWInputToIndex(GLFW_KEY_RIGHT_SHIFT)])
    mods |= GLFW_MOD_SHIFT;
  if (inputState[GLFWInputToIndex(GLFW_KEY_LEFT_CONTROL)] || inputState[GLFWInputToIndex(GLFW_KEY_RIGHT_CONTROL)])
    mods |= GLFW_MOD_CONTROL;
  if (inputState[GLFWInputToIndex(GLFW_KEY_LEFT_ALT)] || inputState[GLFWInputToIndex(GLFW_KEY_RIGHT_ALT)])
    mods |= GLFW_MOD_ALT;
  if (inputState[GLFWInputToIndex(GLFW_KEY_LEFT_SUPER)] || inputState[GLFWInputToIndex(GLFW_KEY_RIGHT_SUPER)])
    mods |= GLFW_MOD_SUPER;
  return mods;
}
auto InputManager::FireAction(unsigned char index) -> void {
  if (!keysEnabled || IsSequenceInProgress())
    return;
  if (pressState[index]) {
    auto actions = pressActions.equal_range(index);
    for (auto it = actions.first; it != actions.second; ++it)
      it->second();
  } else if (releaseState[index]) {
    auto actions = releaseActions.equal_range(index);
    for (auto it = actions.first; it != actions.second; ++it)
      it->second();
  }
}
auto InputManager::IsSequenceInProgress() const -> bool {
  return !keyseq.empty();
}
auto InputManager::GLFWInputToIndex(int key) -> unsigned char {
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
  case GLFW_KEY_MENU:
    return 121;
  }
  if (key >= GLFW_KEY_KP_0 && key <= GLFW_KEY_KP_9)
    return static_cast<unsigned char>(104 + key - GLFW_KEY_KP_0);
  if (key >= GLFW_MOUSE_BUTTON_1 && key <= GLFW_MOUSE_BUTTON_8)
    return static_cast<unsigned char>(122 + key - GLFW_MOUSE_BUTTON_1);
  return 255;
}
auto InputManager::GLFWKeyToString(int key) -> std::string {
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
  case GLFW_KEY_MENU:
    return "Menu";
  }
  if (key >= GLFW_KEY_KP_0 && key <= GLFW_KEY_KP_9)
    return "Keypad" + std::to_string(key - GLFW_KEY_KP_0);
  if (key >= GLFW_MOUSE_BUTTON_1 && key <= GLFW_MOUSE_BUTTON_8)
    return "MouseButton" + std::to_string(1 + key - GLFW_MOUSE_BUTTON_1);
  return "Unknown";
}
} // namespace kuki
