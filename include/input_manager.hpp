#pragma once
#include <functional>
#include <GLFW/glfw3.h>
#include <glm/ext/vector_float2.hpp>
#include <string>
#include <unordered_map>
#include <unordered_set>
class InputManager {
private:
  InputManager();
  InputManager(const InputManager&) = delete;
  InputManager& operator=(const InputManager&) = delete;
  void Init();
  bool keysEnabled;
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
  static void KeyCallback(GLFWwindow*, int, int, int, int);
  static void MouseButtonCallback(GLFWwindow*, int, int, int);
  static void CursorPosCallback(GLFWwindow*, double, double);
public:
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
