#pragma once
#include <bitset>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_int2.hpp>
#include <kuki_engine_export.h>
#include <manager.hpp>
#include <string>
#include <trie.hpp>
#include <unordered_map>
#include <vector>
struct GLFWwindow;
namespace kuki {
class KUKI_ENGINE_API InputManager final : public Manager {
public:
  using ActionID = size_t;
  static constexpr ActionID InvalidActionID = static_cast<ActionID>(-1);
  struct Trigger {
    int key{};
    int mods{};
    auto operator==(const Trigger &) const -> bool = default;
  };
  enum class CaptureState : uint8_t { Pending,
    Cancelled,
    Captured };
  struct CaptureOutcome {
    CaptureState state{CaptureState::Pending};
    Trigger trigger{};
  };
  InputManager(Application &);
  auto BeginKeyCapture() -> void;
  auto CancelKeyCapture() -> void;
  auto CharCallback(GLFWwindow *, unsigned int) -> void;
  auto CursorPosCallback(GLFWwindow *, double, double) -> void;
  auto DisableAll() -> void;
  auto DisableButtons() -> void;
  auto DisableKeys() -> void;
  auto EnableAll() -> void;
  auto EnableButtons() -> void;
  auto EnableKeys() -> void;
  auto GetArrowKeys() const -> glm::ivec2;
  auto GetBinding(const std::string &) const -> Trigger;
  auto GetBindingDescription(const std::string &) const -> std::string;
  auto GetBindingNames() const -> const std::vector<std::string> &;
  auto GetInactivityTime() const -> double;
  auto GetMousePosition() const -> glm::vec2;
  auto GetScrollOffset() const -> glm::vec2;
  auto GetSequenceDescription(const std::string &) const -> std::string;
  auto GetSequenceNames() const -> const std::vector<std::string> &;
  auto GetState(int) const -> bool;
  auto GetTriggerName(const Trigger &) const -> std::string;
  auto GetWASD() const -> glm::ivec2;
  auto IsBindingHeld(const std::string &) const -> bool;
  auto IsBindingPressed(const std::string &) const -> bool;
  auto IsPressed(int) const -> bool;
  auto IsReleased(int) const -> bool;
  auto IsSequenceInProgress() const -> bool;
  auto KeyCallback(GLFWwindow *, int, int, int, int) -> void;
  auto MouseButtonCallback(GLFWwindow *, int, int, int) -> void;
  auto PollKeyCapture() -> CaptureOutcome;
  auto RegisterAction(int, InputAction, bool = true) -> ActionID;
  auto RegisterAction(const std::string &, InputAction, std::string = "") -> ActionID;
  auto RegisterBinding(const std::string &, const Trigger &, std::string = "") -> void;
  auto SetBinding(const std::string &, const Trigger &) -> void;
  auto UnregisterAction(ActionID) -> bool;
  auto ResetPulses() -> void;
  auto ResetScroll() -> void;
  auto ScrollCallback(GLFWwindow *, double, double) -> void;
private:
  struct Registration {
    enum class Kind : uint8_t { Press,
      Release,
      Sequence } kind{Kind::Press};
    std::unordered_multimap<unsigned char, InputAction>::iterator mapIt{};
    std::string sequence;
  };
  Trie<ActionNode> keymap;
  bool buttonsEnabled{true};
  bool capturing{};
  bool captureResolved{};
  bool keysEnabled{true};
  double lastInputTime{};
  glm::vec2 mousePosition{};
  glm::vec2 scrollOffset{};
  ActionID nextActionId{};
  CaptureOutcome capturedResult{};
  std::bitset<256> inputState{0};
  std::bitset<256> pressState{0};
  std::bitset<256> releaseState{0};
  std::unordered_map<ActionID, Registration> idToRegistration;
  std::unordered_map<std::string, Trigger> nameToBinding;
  std::unordered_map<std::string, std::string> nameToDescription;
  std::vector<std::string> bindingOrder;
  std::unordered_multimap<unsigned char, InputAction> pressActions;
  std::unordered_multimap<unsigned char, InputAction> releaseActions;
  std::vector<unsigned char> keyseq;
  std::unordered_map<std::string, ActionID> sequenceToId;
  std::unordered_map<std::string, std::string> sequenceToDescription;
  std::vector<std::string> sequenceOrder;
  auto AssignBinding(const std::string &, const Trigger &) -> void;
  auto CurrentMods() const -> int;
  auto FireAction(unsigned char) -> void;
  auto RegisterSequence(std::string, InputAction, std::string = "") -> ActionID;
  static auto GLFWInputToIndex(int) -> unsigned char;
  static auto GLFWKeyToString(int) -> std::string;
};
} // namespace kuki
