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
  /// @brief Which half of the input space a call means, or both halves at once.
  ///
  /// Halves rather than kinds, because underneath there is only one space: keys and mouse buttons
  /// share `inputState`, indexed by `GLFWInputToIndex`, and what separates them is which flag gates
  /// their callback rather than where their state is kept.
  enum class InputKind : uint8_t { Keys,
    Buttons,
    All };
  /// @brief A quad of keys read as a two-axis direction.
  ///
  /// The two quads had a method each and the two methods were the same nine lines with four
  /// different key codes in them, which is what makes this a parameter rather than a name.
  enum class KeyAxis : uint8_t { Arrows,
    WASD };
  InputManager(Application &);
  auto BeginKeyCapture() -> void;
  auto CancelKeyCapture() -> void;
  /// @brief Feeds a printable character into the key-sequence trie, resetting the sequence when it stops matching a known prefix.
  ///
  /// An action bound to the completed sequence fires during the trie traversal itself, not on return.
  auto CharCallback(GLFWwindow *, unsigned int) -> void;
  auto CursorPosCallback(GLFWwindow *, double, double) -> void;
  auto GetBinding(const std::string &) const -> Trigger;
  auto GetBindingDescription(const std::string &) const -> std::string;
  auto GetBindingNames() const -> const std::vector<std::string> &;
  auto GetInactivityTime() const -> double;
  /// @brief The direction a key quad is currently pointing, as -1, 0 or 1 per axis.
  ///
  /// Opposing keys held together read as nought on that axis rather than as whichever was pressed
  /// first, which is the behaviour both quads had and is what keeps a stuck key from steering.
  auto GetKeyAxis(const KeyAxis) const -> glm::ivec2;
  auto GetMousePosition() const -> glm::vec2;
  auto GetScrollOffset() const -> glm::vec2;
  auto GetSequenceDescription(const std::string &) const -> std::string;
  auto GetSequenceNames() const -> const std::vector<std::string> &;
  auto GetState(int) const -> bool;
  auto GetTriggerName(const Trigger &) const -> std::string;
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
  /// @brief Turns one half of the input space on or off, or both.
  ///
  /// In place of six `EnableX`/`DisableX` methods over two booleans. The shape is the one
  /// `Renderer::SetPassEnabled` already uses, and it puts the asymmetry below somewhere it can be
  /// read rather than in whichever of the six bodies you happened to open.
  ///
  /// `All` also clears a half-typed key sequence and the single-half cases do not. That is the
  /// behaviour as it stood, kept deliberately rather than tidied, but it is worth knowing: the
  /// editor turns keys off while Dear ImGui wants text, so a sequence somebody had started survives
  /// across the text field and can still complete on the first character after keys come back.
  auto SetEnabled(const InputKind, const bool) -> void;
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
  /// @brief Runs the actions registered for the event that has just arrived.
  ///
  /// Told which event it was rather than reading the pulse flags, because those are sticky for the
  /// whole frame: a press and a release inside one frame left the press flag still set when the
  /// release arrived, and the release actions never ran at all. A click faster than a frame then
  /// looked to a script like a button that went down and stayed down.
  auto FireAction(unsigned char, const bool) -> void;
  auto RegisterSequence(std::string, InputAction, std::string = "") -> ActionID;
  static auto GLFWInputToIndex(int) -> unsigned char;
  static auto GLFWKeyToString(int) -> std::string;
};
} // namespace kuki
