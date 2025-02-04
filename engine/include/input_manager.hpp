#pragma once
#include <engine_export.h>
#include <functional>
#include <GLFW/glfw3.h>
#include <glm/ext/vector_float2.hpp>
#include <string>
#include <unordered_map>
#include <unordered_set>
class ENGINE_EXPORT InputManager {
private:
  InputManager() = default;
  bool keysEnabled = true;
  bool updateBindings = false;
  double lastInputTime = .0;
  glm::vec2 mousePos = glm::vec2(.0f);
  std::unordered_map<int, bool> keyStates{{GLFW_KEY_W, false}, {GLFW_KEY_S, false}, {GLFW_KEY_A, false}, {GLFW_KEY_D, false}, {GLFW_KEY_UP, false}, {GLFW_KEY_DOWN, false}, {GLFW_KEY_LEFT, false}, {GLFW_KEY_RIGHT, false}};
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
  static void KeyCallback(GLFWwindow*, int, int, int, int);
  static void MouseButtonCallback(GLFWwindow*, int, int, int);
  static void CursorPosCallback(GLFWwindow*, double, double);
public:
  InputManager(const InputManager&) = delete;
  InputManager& operator=(const InputManager&) = delete;
  InputManager(InputManager&&) = delete;
  InputManager& operator=(InputManager&&) = delete;
  static InputManager& GetInstance();
  void SetWindowCallbacks(GLFWwindow*);
  bool GetKey(int);
  bool GetButton(int);
  glm::vec2 GetWASD();
  glm::vec2 GetArrow();
  glm::vec2 GetMousePos() const;
  double GetInactivityTime() const;
  void RegisterCallback(int, int, std::function<void()>, std::string = "");
  void UnregisterCallback(int, int);
  void RegisterKey(int, std::string = "");
  void UnregisterKey(int);
  const std::unordered_map<std::string, std::string>& GetKeyBindings();
  void DisableCallback(int);
  void EnableCallback(int);
  void DisableKeyCallbacks();
  void EnableKeyCallbacks();
};
