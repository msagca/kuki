#pragma once
#include <bitset>
#include <cstdint>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_int2.hpp>
#include <kuki_engine_export.h>
#include <manager.hpp>
#include <optional>
#include <string>
#include <string_view>
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
  /// @brief Marks the start of a stretch whose bindings, buttons and actions are to be undone.
  ///
  /// A game run inside an editor registers what it needs as it starts, the same as it does on its
  /// own, and that is not harmless there: a binding takes its key from whoever had it, so a game
  /// that steps with A leaves the editor's own A unbound. `EndSession` puts every binding back as
  /// it stood here, forgets the buttons, and unregisters each action registered since.
  auto BeginSession() -> void;
  auto EndSession() -> void;
  auto CancelKeyCapture() -> void;
  /// @brief Feeds a printable character into the key-sequence trie, resetting the sequence when it stops matching a known prefix.
  ///
  /// An action bound to the completed sequence fires during the trie traversal itself, not on return.
  auto CharCallback(GLFWwindow *, unsigned int) -> void;
  auto CursorPosCallback(GLFWwindow *, double, double) -> void;
  auto GetBinding(const std::string &) const -> Trigger;
  /// @brief The trigger a binding was registered with, which is what resetting the controls puts back.
  ///
  /// Kept apart from the binding itself because a registration may be handed a saved trigger to use
  /// instead -- see `RegisterBinding` -- and then the two differ from the first frame.
  auto GetDefaultBinding(const std::string &) const -> Trigger;
  auto GetBindingDescription(const std::string &) const -> std::string;
  auto GetBindingNames() const -> const std::vector<std::string> &;
  /// @brief Which registered buttons are down for the tick being run, a bit each in the order they were registered.
  ///
  /// As of the last `LatchTick`, and unchanged until the next: every reader inside one tick gets the
  /// same answer however the keyboard moves meanwhile. See `ButtonMask` in `gesture.hpp` for why the
  /// whole of a tick's input is this one number.
  auto GetButtons() const -> uint32_t;
  auto GetButtonNames() const -> const std::vector<std::string> &;
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
  /// @brief Fixes what the keyboard is taken to be doing for the tick about to run.
  ///
  /// A key counts as down for a tick if it is down now or went down at any point since the last
  /// tick. The second half is what keeps a tap: a key struck and let go between two ticks -- inside
  /// one frame, or across several on a display faster than the tick rate -- is down at neither
  /// moment a tick looks, and sampling the state alone would never see it. Latched, it is down for
  /// exactly one tick and up for the next, which is a press and a release to anything reading the
  /// masks.
  ///
  /// It does cost a little truth about when. GLFW reports a key when it is polled, not when it was
  /// struck, so everything that arrived during a frame lands on the first tick of the next and the
  /// order of two keys inside one frame is lost. Timing finer than a frame would take reading the
  /// keyboard on its own thread.
  auto LatchTick() -> void;
  auto MouseButtonCallback(GLFWwindow *, int, int, int) -> void;
  auto PollKeyCapture() -> CaptureOutcome;
  auto RegisterAction(int, InputAction, bool = true) -> ActionID;
  auto RegisterAction(const std::string &, InputAction, std::string = "") -> ActionID;
  /// @brief Registers a named binding, on `saved` if one is given and on the trigger otherwise.
  ///
  /// The trigger is the default either way, and is what `GetDefaultBinding` answers. A saved trigger
  /// is taken at registration rather than applied after it, because applying it after would let the
  /// default claim its key first: a player who swapped punch and kick would find the kick's default
  /// taking the key the punch had been saved on, and the punch left with nothing.
  auto RegisterBinding(const std::string &, const Trigger &, std::string = "", const std::optional<Trigger> & = std::nullopt) -> void;
  /// @brief Registers a binding that is also one of the game's buttons, and answers its bit.
  ///
  /// A button is a binding and nothing less: it has a name, it is rebound the same way, and it loses
  /// its key to whoever claims it next. What it has in addition is a place in the mask `GetButtons`
  /// returns, which is the form the fixed tick reads input in. Bits are given out in the order of
  /// registration and that order is part of the game's protocol -- two peers have to register the
  /// same buttons in the same order for their masks to mean the same thing.
  ///
  /// Registering a name again answers the bit it already has. Past thirty-two the answer is -1.
  auto RegisterButton(const std::string &, const Trigger &, std::string = "", const std::optional<Trigger> & = std::nullopt) -> int;
  /// @brief Lets go of everything held, as though every key and button had come up at once.
  ///
  /// For a menu taking the keyboard away from the game: what the game was holding when it opened is
  /// released then, rather than left down until a release the menu has kept for itself. Release
  /// actions run, so a drag that was under way ends the way a drag ends.
  auto ReleaseAll() -> void;
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
  std::bitset<256> tickLatch{0};
  uint32_t tickButtons{};
  std::vector<std::string> buttonOrder;
  struct Session {
    bool active{};
    ActionID firstAction{};
    std::unordered_map<std::string, Trigger> nameToBinding;
    std::unordered_map<std::string, std::string> nameToDescription;
    std::vector<std::string> bindingOrder;
    std::vector<std::string> buttonOrder;
  } session;
  std::unordered_map<ActionID, Registration> idToRegistration;
  std::unordered_map<std::string, Trigger> nameToBinding;
  std::unordered_map<std::string, std::string> nameToDescription;
  std::unordered_map<std::string, Trigger> nameToDefault;
  std::vector<std::string> bindingOrder;
  std::unordered_multimap<unsigned char, InputAction> pressActions;
  std::unordered_multimap<unsigned char, InputAction> releaseActions;
  std::vector<unsigned char> keyseq;
  std::unordered_map<std::string, ActionID> sequenceToId;
  std::unordered_map<std::string, std::string> sequenceToDescription;
  std::vector<std::string> sequenceOrder;
  auto AssignBinding(const std::string &, const Trigger &) -> void;
  auto CurrentMods() const -> int;
  /// @brief Whether the held modifiers are this trigger's, given every binding on its key.
  ///
  /// The modifiers a trigger names must be held, and that alone was the test: so a trigger with
  /// none was satisfied whatever was held, and Ctrl+C, which is Copy, was also C, which toggled the
  /// FPS counter. Requiring exactly the trigger's modifiers instead breaks two things that work --
  /// a button held while a modifier is down for something else, and a binding whose key is itself
  /// a modifier, which sets its own bit by being held. So the most particular binding wins: a
  /// trigger yields only to another binding on the same key that names all of its modifiers and
  /// more, and has them all held.
  auto ModsSatisfy(const Trigger &, const int mods) const -> bool;
  /// @brief Runs the actions registered for the event that has just arrived.
  ///
  /// Told which event it was rather than reading the pulse flags, because those are sticky for the
  /// whole frame: a press and a release inside one frame left the press flag still set when the
  /// release arrived, and the release actions never ran at all. A click faster than a frame then
  /// looked to a game like a button that went down and stayed down.
  auto FireAction(unsigned char, const bool) -> void;
  auto RegisterSequence(std::string, InputAction, std::string = "") -> ActionID;
  static auto GLFWInputToIndex(int) -> unsigned char;
  static auto GLFWKeyToString(int) -> std::string;
};
/// @brief A trigger as the text it is saved in, `"<key> <mods>"` in GLFW's numbers, and back.
///
/// Numbers rather than key names, because the names are for reading and are not unique the other
/// way round on every layout, and the numbers are what GLFW will hand back next run. Nought for the
/// key is a binding left unbound, which is worth saving too: a binding whose key was taken by
/// another is unbound by choice, and loading the default in its place would take the key back.
auto KUKI_ENGINE_API FormatTrigger(const InputManager::Trigger &) -> std::string;
auto KUKI_ENGINE_API ParseTrigger(const std::string_view) -> std::optional<InputManager::Trigger>;
} // namespace kuki
