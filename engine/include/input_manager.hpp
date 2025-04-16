#pragma once
#include <functional>
#include <GLFW/glfw3.h>
#include <glm/ext/vector_float2.hpp>
#include <kuki_export.h>
#include <string>
#include <unordered_map>
#include <unordered_set>
namespace kuki {
class KUKI_API InputManager {
private:
  bool keysEnabled{true};
  bool updateBindings{false};
  double lastInputTime{};
  glm::vec2 mousePos{};
  std::unordered_map<int, int> keyStates;
  std::unordered_map<int, int> buttonStates;
  std::unordered_map<int, std::function<void()>> pressCallbacks;
  std::unordered_map<int, std::function<void()>> releaseCallbacks;
  std::unordered_set<int> disabledKeys;
  std::unordered_map<int, std::string> keyDescriptions;
  std::unordered_map<std::string, std::string> keyBindings;
  void SetKeyState(int, int);
  void SetButtonState(int, int);
  void SetMousePos(double, double);
  std::string GLFWKeyToString(int);
  void DisableKey(int);
  void EnableKey(int);
public:
  /// @return true if the key is pressed or repeated, false otherwise
  bool GetKey(int) const;
  /// @return true if the button is pressed or repeated, false otherwise
  bool GetButton(int) const;
  /// @return true if the key is pressed, false otherwise
  bool GetKeyDown(int) const;
  /// @return true if the button is pressed, false otherwise
  bool GetButtonDown(int) const;
  /// @return true if the key is released, false otherwise
  bool GetKeyUp(int) const;
  /// @return true if the button is released, false otherwise
  bool GetButtonUp(int) const;
  /// @brief Get the vertical (W-S) and horizontal (A-D) input respectively as a 2D vector
  /// @return A float for each axis with a value of either 0, 1 (up/right) or -1 (down/left)
  glm::vec2 GetWASD() const;
  glm::vec2 GetArrow() const;
  glm::vec2 GetMousePos() const;
  /// @return Time passed since last user input
  double GetInactivityTime() const;
  /// @brief Register a function to be called when the specified key-action combination is observed
  void RegisterCallback(int, int, std::function<void()>, std::string = "");
  /// @brief Unregister the callback function for the given key-action combination
  void UnregisterCallback(int, int);
  const std::unordered_map<std::string, std::string>& GetKeyBindings();
  template <typename... T>
  void DisableKeys(T...);
  template <typename... T>
  void EnableKeys(T...);
  void DisableAllKeys();
  void EnableAllKeys();
  void KeyCallback(GLFWwindow*, int, int, int, int);
  void MouseButtonCallback(GLFWwindow*, int, int, int);
  void CursorPosCallback(GLFWwindow*, double, double);
};
template <typename... T>
void InputManager::DisableKeys(T... args) {
  (DisableKey(args), ...);
}
template <typename... T>
void InputManager::EnableKeys(T... args) {
  (EnableKey(args), ...);
}
} // namespace kuki
