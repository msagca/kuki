#pragma once
#include <cstddef>
#include <cstdint>
#include <engine_config.hpp>
#include <functional>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float4.hpp>
#include <input_manager.hpp>
#include <kuki_engine_export.h>
#include <string>
#include <vector>
namespace kuki {
class Application;
/// @brief Which page of the settings menu an entry is listed on.
///
/// `Game` is the game's own; `Graphics` is the engine's, filled in by the engine; `Controls` is
/// every binding the application has registered, listed by the menu itself and never added to.
enum class SettingsPage : uint8_t {
  Game,
  Graphics,
  Controls
};
/// @brief One of the values a choice can take: its name, and optionally a few colours shown beside
/// it, which is how a board's look is chosen by the look rather than by the word for it.
struct SettingsOption {
  std::string label;
  std::vector<glm::vec4> swatches;
};
/// @brief The settings every game has: a page of its own, the engine's graphics, and its keys, laid
/// over the dimmed picture in the overlay's text, opened and closed by the `Settings` binding.
///
/// Why it is the engine's and not each game's. Every game wants the same three things from it --
/// somewhere to change what it offers, somewhere to change how it is drawn, and somewhere to change
/// its keys -- and a game that drew its own options on the scene, as chess did, spent the screen on
/// them for the whole of every match. A key printed along the bottom of the picture, as the frontier
/// did, is a key that cannot be changed without that line becoming a lie. So hints are gone from the
/// scenes and the keys are listed here instead, by what their bindings are called, rebindable, and
/// always telling the truth about themselves.
///
/// While it is open the game hears nothing: the keys, the buttons, the wheel and the typed
/// characters are kept from the input manager, and whatever the game was holding is let go of as it
/// opens (`InputManager::ReleaseAll`). The cursor still moves. A game that runs alone is paused
/// while it is open -- no tick is run, and `Application::GetPlayState` says `Paused`, which is what a
/// game whose clock runs in `Update` should look at -- and one that is being played against another
/// machine is not, because the other machine is not waiting; it plays on with no buttons held.
///
/// It is the overlay's like everything else on the picture, drawn after every system's `Update` and
/// `LateUpdate` (`SystemApplication::UpdateSystems`), and while it is open it is the only thing the
/// overlay draws: whatever the systems queued that frame is dropped, so the menu stands over the
/// scene and nothing else. It is picked at with `Overlay::HitTest` under ids well clear of any a
/// game would choose. It hangs from the top of the window and grows down, so the title and the
/// pages stay where they are as the pages change.
///
/// An entry is three functions rather than a pointer to a value: what it shows, what choosing does,
/// and whether it is shown at all, asked every frame it is open. That keeps the value where the game
/// already keeps it, and lets choosing do whatever choosing should -- restart a match, save a
/// preference -- without the menu knowing that it does.
class KUKI_ENGINE_API SettingsMenu {
public:
  /// @brief The binding that opens and closes the menu, Escape unless rebound. It cannot be given
  /// away: a key it holds is refused to every other binding, or the menu could not be reached again.
  static constexpr const char *BINDING = "Settings";
  explicit SettingsMenu(Application &);
  /// @brief A value picked from a list. `get` answers which is in force, `set` is told which was
  /// picked. Shown while `visible` is empty or says so.
  auto AddChoice(const SettingsPage, std::string, std::vector<SettingsOption>, std::function<size_t()>, std::function<void(size_t)>, std::function<bool()> = {}) -> void;
  /// @brief A choice between off and on.
  auto AddToggle(const SettingsPage, std::string, std::function<bool()>, std::function<void(bool)>, std::function<bool()> = {}) -> void;
  /// @brief Something done rather than set: a new game, a new map. The menu closes once it is done.
  auto AddAction(const SettingsPage, std::string, std::function<void()>, std::function<bool()> = {}) -> void;
  /// @brief Forgets every entry. On the way out, since the entries call into systems that are going.
  auto Clear() -> void;
  auto IsEnabled() const -> bool;
  /// @brief Whether the menu can be opened at all. The editor's is off: it has panels of its own.
  auto SetEnabled(const bool) -> void;
  auto IsOpen() const -> bool;
  auto Open() -> void;
  auto Close() -> void;
private:
  friend class Application;
  enum class EntryKind : uint8_t {
    Choice,
    Toggle,
    Action
  };
  struct Entry {
    SettingsPage page{};
    EntryKind kind{};
    std::string label;
    std::vector<SettingsOption> options;
    std::function<size_t()> get;
    std::function<void(size_t)> set;
    std::function<bool()> visible;
  };
  /// @brief A line of the page being shown: one of the entries, one of the bindings, or the line
  /// that puts every binding back.
  struct Row {
    enum class Kind : uint8_t {
      Entry,
      Binding,
      ResetBindings
    } kind{};
    size_t entry{};
    std::string binding;
  };
  /// @brief Something the window reported while the menu had it, kept for `Update` to act on.
  ///
  /// Kept rather than acted on where it arrives, because it arrives inside `glfwPollEvents`, and what
  /// an entry does -- restarting a match, building a map -- is not something to do in the middle of
  /// the input dispatch. The duel defers its own keys for the same reason.
  struct Event {
    enum class Kind : uint8_t {
      Key,
      KeyUp,
      Click,
      RightClick,
      Scroll
    } kind{};
    int key{};
    int mods{};
    float scroll{};
  };
  Application &app;
  std::vector<Entry> entries;
  std::vector<Event> events;
  bool enabled{true};
  bool open{};
  /// @brief Whether opening the menu paused the game, and so whether closing it should play it again.
  bool pausedGame{};
  SettingsPage page{SettingsPage::Game};
  /// @brief The line keyboard focus is on: the row of pages above everything (`TAB_ROW`), a row of
  /// the page, or past the rows, the footer's two. The arrow keys reach all of them, so the whole
  /// menu can be worked without the mouse -- which a game played on the keyboard needs.
  int focus{};
  static constexpr int TAB_ROW = -1;
  /// @brief How many rows the page is scrolled past, when it has more than fit.
  int scroll{};
  /// @brief The binding waiting for a key, or empty when none is.
  std::string capturing;
  /// @brief A modifier pressed while waiting for a key, which becomes the key itself if it comes back
  /// up with nothing pressed meanwhile -- the only way to bind Shift on its own.
  int pendingModifier{};
  /// @brief A line said under the rows for a moment, and how much longer it is said for.
  std::string notice;
  float noticeLeft{};
  glm::vec2 lastMouse{};
  /// @brief How many rows the last frame had room for, and whether the keyboard has moved the focus
  /// since, so the page should scroll to keep it in view. The wheel scrolls without moving it.
  int visibleRows{1};
  bool revealFocus{};
  /// @brief The graphics settings as last chosen. Read once at startup and written on every change.
  EngineConfig graphics;
  /// @brief Puts `graphics` into effect on the renderer and the window.
  auto ApplyGraphics() -> void;
  /// @brief Writes the fields this menu owns into the config file, leaving the rest as they are.
  auto SaveGraphics() const -> void;
  /// @brief Applies the saved graphics settings and lists them on the graphics page. After the
  /// systems have started, so the renderer they reach is there.
  auto Start() -> void;
  /// @brief Acts on what arrived since the last frame and queues the menu into the overlay.
  auto Update(const float) -> void;
  /// @brief The window's events, offered to the menu before the input manager sees them. Each says
  /// whether it kept the event.
  auto OnKey(const int, const int, const int) -> bool;
  auto OnChar() const -> bool;
  auto OnMouseButton(const int, const int) -> bool;
  auto OnScroll(const double) -> bool;
  auto Pages() const -> std::vector<SettingsPage>;
  auto BuildRows() const -> std::vector<Row>;
  auto Handle(const Event &, const std::vector<Row> &) -> void;
  auto Activate(const int, const std::vector<Row> &) -> void;
  auto Step(const Row &, const int) -> void;
  auto Capture(const InputManager::Trigger &) -> void;
  auto ShowNotice(std::string) -> void;
  auto SwitchPage(const SettingsPage) -> void;
  /// @brief Moves to the page before or after the one shown, wrapping round.
  auto CyclePage(const int) -> void;
  auto Draw(const std::vector<Row> &) -> void;
  auto ValueText(const Row &) const -> std::string;
  /// @brief Writes every binding that differs from its default into the preferences, and removes the
  /// rest, so that what is saved is exactly what was changed.
  auto SaveBindings() -> void;
  auto AddGraphicsEntries() -> void;
};
} // namespace kuki
