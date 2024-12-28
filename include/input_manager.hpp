#pragma once
#include <cstdint>
#include <functional>
#include <GLFW/glfw3.h>
#include <glm/ext/vector_float2.hpp>
#include <unordered_map>
#include <unordered_set>
class InputManager {
public:
  static InputManager& GetInstance() {
    static InputManager instance;
    return instance;
  }
  void Initialize(GLFWwindow*);
  bool GetKey(int);
  bool GetKey(int, int);
  bool GetButton(int);
  glm::vec2 GetWASD() const;
  glm::vec2 GetArrow() const;
  glm::vec2 GetMousePos() const;
  double GetInactivityTime() const;
  void RegisterCallback(int, int, std::function<void()>);
  void UnregisterCallback(int, int);
  void DisableCallback(int);
  void EnableCallback(int);
  void DisableKeyCallbacks();
  void EnableKeyCallbacks();
private:
  InputManager() = default;
  InputManager(const InputManager&) = delete;
  InputManager& operator=(const InputManager&) = delete;
  GLFWwindow* window = nullptr;
  double lastInputTime = .0;
  uint32_t keyMask = 0;
  glm::vec2 mousePos = glm::vec2(.0f);
  void SetKeyState(int, int);
  void SetButtonState(int, int);
  void SetMousePos(double, double);
  uint32_t KeyToBitmask(int);
  std::unordered_map<int, std::function<void()>> pressCallbacks;
  std::unordered_map<int, std::function<void()>> releaseCallbacks;
  std::unordered_set<int> inactiveCallbacks;
  bool keysEnabled = true;
  static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    GetInstance().SetKeyState(key, action);
  }
  static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    GetInstance().SetButtonState(button, action);
  }
  static void CursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    GetInstance().SetMousePos(xpos, ypos);
  }
};
