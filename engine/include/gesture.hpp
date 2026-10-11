#pragma once
#include <array>
#include <cstdint>
#include <kuki_engine_export.h>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
namespace kuki {
/// @brief Which of a game's buttons are down during one tick, a bit each.
///
/// The whole of what a player contributes to a tick, and deliberately this small. Everything else
/// that could be said about input -- what was pressed, what was let go, how long something has been
/// held, which gesture just finished -- follows from a run of these and nothing else, so two
/// machines given the same run reach the same answers. That makes this the thing to send to a peer,
/// to write into a replay, and to feed back in when a tick has to be run again.
using ButtonMask = uint32_t;
inline constexpr int MAX_BUTTONS = 32;
using GestureID = uint32_t;
inline constexpr GestureID INVALID_GESTURE = static_cast<GestureID>(-1);
enum class GestureStepKind : uint8_t {
  /// The buttons go down. More than one is a chord: all of them down, the last arriving now and none
  /// of them more than `GestureRecognizer::CHORD_WINDOW` ticks before it.
  Press,
  /// The buttons have all been down together for `ticks` ticks.
  Hold,
  /// The buttons were all down and one of them has just come up.
  Release
};
struct GestureStep {
  ButtonMask buttons{};
  /// @brief Buttons that only have to be down while the step happens, whenever they went down.
  ///
  /// A modifier, as opposed to part of a chord. A chord is a thing done at one moment, so its keys
  /// have to arrive together; a modifier is a state the player is already in.
  ButtonMask with{};
  GestureStepKind kind{GestureStepKind::Press};
  /// @brief For `Hold`, how long. Ignored otherwise.
  uint16_t ticks{};
};
struct Gesture {
  /// @brief How long after one step the next may begin, in ticks, unless a gesture says otherwise.
  ///
  /// A quarter of a second at sixty ticks, which is about where a sequence stops feeling like one
  /// movement and starts feeling like two.
  static constexpr uint16_t DEFAULT_WINDOW = 15;
  std::string name;
  std::vector<GestureStep> steps;
  uint16_t window{DEFAULT_WINDOW};
  /// @brief Whether pressing a button the next step does not ask for breaks the sequence.
  ///
  /// Off, a sequence is forgiving: "a b" is still made by a, c, b, which is what a player whose
  /// fingers are busy with something else needs. On, it is exact, which is what tells apart two
  /// gestures where one is the other with a step dropped.
  bool strict{};
};
/// @brief Reads steps out of a line of text, given the names of the buttons in bit order.
///
/// Steps are separated by spaces, and each is one of:
///
///   `punch`         the button is pressed
///   `left+right`    a chord: both pressed together
///   `guard*30`      held for thirty ticks
///   `-punch`        released
///   `guard/punch`   pressed while `guard` is already down
///
/// and the forms combine: `guard/-left+right` is the chord let go with `guard` held. So a double tap
/// is `punch punch`, and charging something and letting it fly is `punch*40 -punch`.
///
/// Empty if the text names a button that is not in the list or does not parse.
KUKI_ENGINE_API auto ParseGesture(std::string_view, std::span<const std::string>) -> std::optional<std::vector<GestureStep>>;
/// @brief Finds gestures in a run of button masks, one mask a tick.
///
/// It keeps no state but the masks themselves. Each tick it asks of every gesture whether its last
/// step has just happened, and if so whether the step before it happened within the window before
/// that, and so on back to the first. Matching backwards from the present, rather than carrying a
/// half-finished state machine per gesture forwards, is what lets gestures overlap freely: the end
/// of one can be the start of another, and a step can be attempted twice, without either having to
/// be thought of in advance.
///
/// It is also what makes it exact to reproduce. There is one of these per player, local or remote,
/// and one fed the same masks in the same order gives the same gestures on the same ticks.
///
/// Counted in ticks throughout, never seconds, so a window is a different length of time if the
/// tick rate changes. That is deliberate: the alternative is rounding a time to a tick somewhere,
/// and that is a second place for two machines to disagree.
class KUKI_ENGINE_API GestureRecognizer {
public:
  /// @brief How far apart the keys of a chord may land and still be one chord, in ticks.
  static constexpr int CHORD_WINDOW = 3;
  /// @brief How many ticks back the recognizer can see. Bounds the longest hold and the widest window.
  static constexpr int HISTORY = 512;
  /// @brief Takes a gesture, or refuses one that could never be made and says why in the log.
  auto Add(Gesture) -> GestureID;
  /// @brief Takes a gesture written in the notation `ParseGesture` reads.
  auto Add(std::string, std::string_view, std::span<const std::string>, const uint16_t = Gesture::DEFAULT_WINDOW, const bool = false) -> GestureID;
  /// @brief Takes the tick's buttons and answers which gestures that completes.
  ///
  /// Most specific first -- more steps, then more buttons, then whichever was added earlier -- so a
  /// caller that wants one answer takes the first. All of them are reported rather than only that
  /// one, because whether a long gesture finishing should also count as the short one it ends with
  /// is a rule of the game and not of the keyboard.
  ///
  /// One thing follows from recognising without delay. A chord whose keys land on different ticks
  /// is preceded, a tick or two earlier, by the gesture for its first key alone. Holding every
  /// single press back for `CHORD_WINDOW` ticks in case a chord follows would hide that, at the cost
  /// of every press in the game being that much late, so it is left to the game: let the chord
  /// override what the first key started.
  ///
  /// The span is good until the next call.
  auto Advance(const ButtonMask) -> std::span<const GestureID>;
  auto Get(const GestureID) const -> const Gesture *;
  /// @brief The buttons as of the last tick.
  auto GetHeld() const -> ButtonMask;
  /// @brief How many ticks in a row every one of these buttons has been down, as of the last tick.
  auto GetHeldTicks(const ButtonMask) const -> int;
  /// @brief Forgets the masks seen so far and keeps the gestures. For the start of a round.
  auto Reset() -> void;
private:
  std::vector<Gesture> gestures;
  std::vector<GestureID> order;
  std::vector<GestureID> matched;
  std::array<ButtonMask, HISTORY> history{};
  int64_t count{};
  auto At(const int64_t) const -> ButtonMask;
  auto Pressed(const int64_t) const -> ButtonMask;
  auto Released(const int64_t) const -> ButtonMask;
  /// @brief How many ticks, ending at the one given, the buttons have all been down.
  auto Streak(const ButtonMask, const int64_t) const -> int;
  /// @brief Whether a step happens on a tick, and if so the tick it began on.
  ///
  /// The two differ only for a hold, which begins when the buttons went down. A hold in the middle
  /// of a gesture is satisfied on every tick from the one that makes it long enough onwards, so that
  /// "hold, then release" works however long the hold went on; one at the end is satisfied on that
  /// first tick alone, so that it is reported once.
  auto Completes(const Gesture &, const size_t, const int64_t, int64_t &) const -> bool;
  auto Match(const Gesture &, const size_t, const int64_t) const -> bool;
};
} // namespace kuki
