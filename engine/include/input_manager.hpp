#pragma once
#include <engine_export.h>
#include <functional>
#include <GLFW/glfw3.h>
#include <glm/ext/vector_float2.hpp>
#include <string>
#include <unordered_map>
#include <unordered_set>
class ENGINE_API InputManager {
private:
  bool keysEnabled = true;
  bool updateBindings;
  double lastInputTime;
  glm::vec2 mousePos;
  std::unordered_map<int, bool> keyStates;
  std::unordered_map<int, bool> buttonStates;
  std::unordered_map<int, std::function<void()>> pressCallbacks;
  std::unordered_map<int, std::function<void()>> releaseCallbacks;
  std::unordered_set<int> inactiveCallbacks;
  std::unordered_map<int, std::string> keyDescriptions;
  std::unordered_map<std::string, std::string> keyBindings;
  void SetKeyState(int, int);
  void SetButtonState(int, int);
  void SetMousePos(double, double);
  std::string GLFWKeyToString(int);
public:
  /// <returns>true if the key is pressed/repeated, false otherwise</returns>
  bool GetKey(int) const;
  /// <returns>true if the button is pressed/repeated, false otherwise</returns>
  bool GetButton(int) const;
  /// <summary>
  /// Get the vertical (W-S) and horizontal (A-D) input respectively as a 2D vector
  /// </summary>
  /// <returns>a float for each axis with a value of either 0, 1 (up/right) or -1 (down/left)</returns>
  glm::vec2 GetWASD() const;
  glm::vec2 GetArrow() const;
  glm::vec2 GetMousePos() const;
  /// <returns>time passed since last user input</returns>
  double GetInactivityTime() const;
  /// <summary>
  /// Register a function to be called when the specified key-action combination is observed
  /// </summary>
  void RegisterCallback(int, int, std::function<void()>, std::string = "");
  /// <summary>
  /// Unregister the callback function for the given key-action combination
  /// </summary>
  void UnregisterCallback(int, int);
  const std::unordered_map<std::string, std::string>& GetKeyBindings();
  void DisableCallback(int);
  void EnableCallback(int);
  void DisableKeyCallbacks();
  void EnableKeyCallbacks();
  void KeyCallback(GLFWwindow*, int, int, int, int);
  void MouseButtonCallback(GLFWwindow*, int, int, int);
  void CursorPosCallback(GLFWwindow*, double, double);
};
