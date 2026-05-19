#pragma once
#include <bitset>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_int2.hpp>
#include <kuki_engine_export.h>
#include <string>
#include <trie.hpp>
#include <unordered_map>
struct GLFWwindow;
namespace kuki {
class KUKI_ENGINE_API InputManager {
public:
  auto CharCallback(GLFWwindow *, unsigned int) -> void;
  auto CursorPosCallback(GLFWwindow *, double, double) -> void;
  auto DisableAll() -> void;
  auto DisableButtons() -> void;
  auto DisableKeys() -> void;
  auto EnableAll() -> void;
  auto EnableButtons() -> void;
  auto EnableKeys() -> void;
  auto GetArrowKeys() const -> glm::ivec2;
  auto GetInactivityTime() const -> double;
  auto GetMousePosition() const -> glm::vec2;
  auto GetState(int) const -> bool;
  auto GetWASD() const -> glm::ivec2;
  auto IsPressed(int) const -> bool;
  auto IsReleased(int) const -> bool;
  auto KeyCallback(GLFWwindow *, int, int, int, int) -> void;
  auto MouseButtonCallback(GLFWwindow *, int, int, int) -> void;
  auto RegisterAction(int, InputAction, bool = true) -> void;
  auto RegisterAction(const std::string &, InputAction) -> void;
private:
  Trie<ActionNode> keymap;
  bool buttonsEnabled{true};
  bool keysEnabled{true};
  double lastInputTime{};
  glm::vec2 mousePosition{};
  std::bitset<256> inputState{0};
  std::bitset<256> pressState{0};
  std::bitset<256> releaseState{0};
  std::unordered_multimap<unsigned char, InputAction> pressActions;
  std::unordered_multimap<unsigned char, InputAction> releaseActions;
  std::vector<unsigned char> keyseq;
  auto FireAction(unsigned char) -> void;
  auto SequenceInProgress() const -> bool;
  static auto GLFWInputToIndex(int) -> unsigned char;
  static auto GLFWKeyToString(int) -> std::string;
};
} // namespace kuki
