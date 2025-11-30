#pragma once
#include <bitset>
#include <glm/ext/vector_float2.hpp>
#include <kuki_engine_export.h>
#include <string>
#include <trie.hpp>
#include <unordered_map>
struct GLFWwindow;
namespace kuki {
class KUKI_ENGINE_API InputManager {
private:
  double lastInputTime{};
  glm::vec2 mousePos{};
  std::bitset<256> inputState{0};
  std::bitset<256> pressState{0};
  std::bitset<256> releaseState{0};
  std::unordered_map<unsigned char, InputAction> pressActions;
  std::unordered_map<unsigned char, InputAction> releaseActions;
  std::vector<unsigned char> keyseq;
  Trie<ActionNode> keymap;
  bool keysEnabled{true};
  bool buttonsEnabled{true};
  bool SequenceInProgress() const;
  void FireAction(unsigned char);
  /// @returns An index for the given GLFW key or button
  static unsigned char GLFWInputToIndex(int);
  /// @returns A string representation for the given GLFW key or button
  static std::string GLFWKeyToString(int);
public:
  /// @return true if the key/button is pressed or repeated, false otherwise
  bool GetState(int) const;
  /// @return true if the key/button is pressed, false otherwise
  bool IsPressed(int) const;
  /// @return true if the key/button is released, false otherwise
  bool IsReleased(int) const;
  /// @brief Get the vertical (W-S) and horizontal (A-D) input respectively as a 2D vector
  glm::vec2 GetWASD() const;
  /// @brief Get the vertical (Up-Down) and horizontal (Left-Right) input respectively as a 2D vector
  glm::vec2 GetArrow() const;
  glm::vec2 GetMousePos() const;
  /// @return Time passed since last user input
  double GetInactivityTime() const;
  /// @brief Register an action that is triggered by the given character sequence
  /// @returns true if both the trigger and action are valid and there is no collision, false otherwise
  bool RegisterAction(const std::string&, InputAction);
  /// @brief Register an action that is triggered by the given key/button, overwrite any existing action
  void RegisterAction(int, InputAction, bool = true);
  /// @brief Unregister the action associated with the given character sequence
  /// @returns true if a mapping was found and removed, false otherwise
  bool UnregisterAction(const std::string&);
  /// @brief Unregister the action associated with the given key/button
  /// @returns true if a mapping was found and removed, false otherwise
  bool UnregisterAction(int, bool = true);
  void EnableKeys();
  void DisableKeys();
  void EnableButtons();
  void DisableButtons();
  void EnableAll();
  void DisableAll();
  void CharCallback(GLFWwindow*, unsigned int);
  void KeyCallback(GLFWwindow*, int, int, int, int);
  void MouseButtonCallback(GLFWwindow*, int, int, int);
  void CursorPosCallback(GLFWwindow*, double, double);
};
} // namespace kuki
