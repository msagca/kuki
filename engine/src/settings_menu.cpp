#include <GLFW/glfw3.h>
#include <algorithm>
#include <application.hpp>
#include <cctype>
#include <cmath>
#include <command_line.hpp>
#include <engine_config.hpp>
#include <format>
#include <glm/common.hpp>
#include <glm/exponential.hpp>
#include <graphics_context.hpp>
#include <input_manager.hpp>
#include <netplay.hpp>
#include <network_system.hpp>
#include <overlay.hpp>
#include <render_pass.hpp>
#include <rendering_system.hpp>
#include <settings_menu.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
namespace kuki {
namespace {
  /// @brief The ids the menu's text answers `Overlay::HitTest` with, far above any a game chooses.
  ///
  /// A row's id is its place on the page times four plus which part of it was hit, so one number says
  /// both: the row as a whole, or the arrow either side of its value.
  constexpr int MENU_ID = 1 << 24;
  constexpr int BACKDROP_ID = MENU_ID;
  constexpr int TAB_ID = MENU_ID + 1;
  constexpr int RESUME_ID = MENU_ID + 10;
  constexpr int QUIT_ID = MENU_ID + 11;
  constexpr int ROW_ID = MENU_ID + 100;
  constexpr int PART_WHOLE = 0;
  constexpr int PART_PREVIOUS = 1;
  constexpr int PART_NEXT = 2;
  constexpr int RowId(const int row, const int part) {
    return ROW_ID + row * 4 + part;
  }
  /// @brief The menu's measures, in pixels of a picture a thousand and eighty high, as the games' own
  /// text is, so it keeps its proportions on any window.
  constexpr float PANEL_WIDTH = 860.f;
  constexpr float TITLE_SIZE = 42.f;
  constexpr float TAB_SIZE = 25.f;
  constexpr float ROW_SIZE = 25.f;
  constexpr float ROW_HEIGHT = 46.f;
  constexpr float FOOTER_SIZE = 27.f;
  constexpr float NOTICE_SIZE = 20.f;
  constexpr float SIDE = 22.f;
  /// @brief How far below the window's top edge the title stands.
constexpr float TOP_MARGIN = 72.f;
/// @brief Space taken by everything but the rows: the title, the tabs, the notice and the footer.
  constexpr float HEADER_HEIGHT = 150.f;
  constexpr float FOOTER_HEIGHT = 120.f;
  /// @brief How long a notice stays, in seconds.
  constexpr float NOTICE_LENGTH = 2.5f;
  /// @brief The menu's colours. The dark over the picture is heavy enough that white text reads on
  /// any scene, and light enough that the scene still says where the game was left.
  constexpr glm::vec4 BACKDROP{.01f, .012f, .02f, .74f};
  constexpr glm::vec4 TEXT{1.f, 1.f, 1.f, .94f};
  constexpr glm::vec4 DIM{1.f, 1.f, 1.f, .55f};
  constexpr glm::vec4 FAINT{1.f, 1.f, 1.f, .28f};
  constexpr glm::vec4 FOCUS{1.f, 1.f, 1.f, .08f};
  constexpr glm::vec4 RULE{1.f, 1.f, 1.f, .14f};
  /// @brief An amber, for what is waiting on the player: the binding listening for a key, a notice.
  constexpr glm::vec4 ACCENT{1.f, .8f, .32f, 1.f};
  auto IsModifier(const int key) -> bool {
    switch (key) {
    case GLFW_KEY_LEFT_SHIFT:
    case GLFW_KEY_RIGHT_SHIFT:
    case GLFW_KEY_LEFT_CONTROL:
    case GLFW_KEY_RIGHT_CONTROL:
    case GLFW_KEY_LEFT_ALT:
    case GLFW_KEY_RIGHT_ALT:
    case GLFW_KEY_LEFT_SUPER:
    case GLFW_KEY_RIGHT_SUPER:
      return true;
    default:
      return false;
    }
  }
  auto PageName(const SettingsPage page) -> const char * {
    switch (page) {
    case SettingsPage::Game:
      return "GAME";
    case SettingsPage::Graphics:
      return "GRAPHICS";
    case SettingsPage::Controls:
    default:
      return "CONTROLS";
    }
  }
} // namespace
SettingsMenu::SettingsMenu(Application &app)
  : app(app) {}
auto SettingsMenu::AddChoice(const SettingsPage page, std::string label, std::vector<SettingsOption> options, std::function<size_t()> get, std::function<void(size_t)> set, std::function<bool()> visible) -> void {
  if (options.empty() || !get || !set)
    return;
  entries.push_back({page, EntryKind::Choice, std::move(label), std::move(options), std::move(get), std::move(set), std::move(visible)});
}
auto SettingsMenu::AddToggle(const SettingsPage page, std::string label, std::function<bool()> get, std::function<void(bool)> set, std::function<bool()> visible) -> void {
  if (!get || !set)
    return;
  entries.push_back({page, EntryKind::Toggle, std::move(label), {{"Off"}, {"On"}}, [get = std::move(get)] { return get() ? size_t{1} : size_t{0}; }, [set = std::move(set)](const size_t index) { set(index == 1); }, std::move(visible)});
}
auto SettingsMenu::AddAction(const SettingsPage page, std::string label, std::function<void()> run, std::function<bool()> visible) -> void {
  if (!run)
    return;
  entries.push_back({page, EntryKind::Action, std::move(label), {}, [] { return size_t{0}; }, [run = std::move(run)](const size_t) { run(); }, std::move(visible)});
}
auto SettingsMenu::Clear() -> void {
  Close();
  entries.clear();
  events.clear();
}
auto SettingsMenu::IsEnabled() const -> bool {
  return enabled;
}
auto SettingsMenu::SetEnabled(const bool value) -> void {
  if (!value)
    Close();
  enabled = value;
}
auto SettingsMenu::IsOpen() const -> bool {
  return open;
}
auto SettingsMenu::Open() -> void {
  if (!enabled || open)
    return;
  // Without a font nothing of it could be drawn, and a menu nobody can see that has taken the
  // keyboard and paused the game is worse than no menu.
  if (!app.GetOverlay().GetFont().IsLoaded()) {
    spdlog::warn("[Settings] The application set no overlay font, so the settings cannot be shown");
    return;
  }
  open = true;
  const auto pages = Pages();
  page = pages.empty() ? SettingsPage::Graphics : pages.front();
  focus = 0;
  scroll = 0;
  capturing.clear();
  pendingModifier = 0;
  events.clear();
  lastMouse = app.GetMousePosition();
  app.inputManager.ReleaseAll();
  // A game played against another machine cannot stop for this one's menu: the other is not
  // waiting, and a peer that stopped ticking would only fall behind and have to be caught up.
  const auto *network = app.GetNetwork();
  const auto *netplay = network ? network->GetNetplay() : nullptr;
  const auto online = netplay && netplay->IsOnline();
  pausedGame = !online && app.GetPlayState() == PlayState::Playing;
  if (pausedGame)
    app.SetPlayState(PlayState::Paused);
}
auto SettingsMenu::Close() -> void {
  if (!open)
    return;
  open = false;
  capturing.clear();
  pendingModifier = 0;
  if (pausedGame && app.GetPlayState() == PlayState::Paused)
    app.SetPlayState(PlayState::Playing);
  pausedGame = false;
}
auto SettingsMenu::OnKey(const int key, const int action, const int mods) -> bool {
  if (!enabled)
    return false;
  if (!open) {
    if (action != GLFW_PRESS)
      return false;
    const auto trigger = app.inputManager.GetBinding(BINDING);
    if (trigger.key == 0 || key != trigger.key || (mods & trigger.mods) != trigger.mods)
      return false;
    Open();
    return open;
  }
  if (action == GLFW_PRESS || action == GLFW_REPEAT)
    events.push_back({Event::Kind::Key, key, mods});
  else if (action == GLFW_RELEASE)
    events.push_back({Event::Kind::KeyUp, key, mods});
  return true;
}
auto SettingsMenu::OnChar() const -> bool {
  return enabled && open;
}
auto SettingsMenu::OnMouseButton(const int button, const int action) -> bool {
  if (!enabled || !open)
    return false;
  if (action == GLFW_PRESS && button == GLFW_MOUSE_BUTTON_LEFT)
    events.push_back({Event::Kind::Click});
  else if (action == GLFW_PRESS && button == GLFW_MOUSE_BUTTON_RIGHT)
    events.push_back({Event::Kind::RightClick});
  return true;
}
auto SettingsMenu::OnScroll(const double offset) -> bool {
  if (!enabled || !open)
    return false;
  events.push_back({Event::Kind::Scroll, 0, 0, static_cast<float>(offset)});
  return true;
}
auto SettingsMenu::Pages() const -> std::vector<SettingsPage> {
  std::vector<SettingsPage> pages;
  // The game's page only when the game put something on it, so a game with nothing of its own to
  // offer opens on the graphics rather than on an empty page.
  if (std::ranges::any_of(entries, [](const Entry &entry) { return entry.page == SettingsPage::Game && (!entry.visible || entry.visible()); }))
    pages.push_back(SettingsPage::Game);
  pages.push_back(SettingsPage::Graphics);
  if (!app.inputManager.GetBindingNames().empty())
    pages.push_back(SettingsPage::Controls);
  return pages;
}
auto SettingsMenu::BuildRows() const -> std::vector<Row> {
  std::vector<Row> rows;
  if (page == SettingsPage::Controls) {
    for (const auto &name : app.inputManager.GetBindingNames())
      rows.push_back({Row::Kind::Binding, 0, name});
    rows.push_back({Row::Kind::ResetBindings});
    return rows;
  }
  for (size_t index = 0; index < entries.size(); ++index)
    if (entries[index].page == page && (!entries[index].visible || entries[index].visible()))
      rows.push_back({Row::Kind::Entry, index});
  return rows;
}
auto SettingsMenu::Start() -> void {
  graphics = EngineConfig::Load();
  ApplyGraphics();
  AddGraphicsEntries();
  if (const auto name = GetFlagValue(GetCommandLineArguments(), SETTINGS_FLAG); name) {
    Open();
    for (const auto each : Pages()) {
      std::string lower(PageName(each));
      std::ranges::transform(lower, lower.begin(), [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
      if (lower == *name)
        SwitchPage(each);
    }
  }
}
auto SettingsMenu::ApplyGraphics() -> void {
  if (auto *rendering = app.GetRenderingSystem(); rendering) {
    rendering->SetAntiAliasingOverride(graphics.antiAliasing);
    rendering->SetPassEnabled(RenderPass::ShadowMap, graphics.shadows);
    rendering->SetPassEnabled(RenderPass::SpotShadowMap, graphics.shadows);
    rendering->SetPassEnabled(RenderPass::BloomEffect, graphics.bloom);
    rendering->SetPassEnabled(RenderPass::PlanarReflection, graphics.reflections);
    rendering->SetLightingQuality(graphics.lighting);
    if (rendering->GetCapabilities().volumetrics) {
      rendering->SetPassEnabled(RenderPass::VolumetricFog, graphics.volumetrics);
      rendering->SetPassEnabled(RenderPass::VolumetricClouds, graphics.volumetrics);
    }
  }
  if (app.graphicsContext)
    app.graphicsContext->SetVSync(graphics.vsync);
}
auto SettingsMenu::SaveGraphics() const -> void {
  // Read again and only these fields written, so that the backend and the tone mapper -- the
  // editor's to set -- are left as the file has them.
  auto config = EngineConfig::Load();
  config.fullscreen = graphics.fullscreen;
  config.vsync = graphics.vsync;
  config.antiAliasing = graphics.antiAliasing;
  config.shadows = graphics.shadows;
  config.bloom = graphics.bloom;
  config.volumetrics = graphics.volumetrics;
  config.reflections = graphics.reflections;
  config.lighting = graphics.lighting;
  config.showFrameRate = graphics.showFrameRate;
  config.Save();
}
auto SettingsMenu::AddGraphicsEntries() -> void {
  const auto *rendering = app.GetRenderingSystem();
  const auto capabilities = rendering ? rendering->GetCapabilities() : RendererCapabilities{};
  // A toggle over one of the flags above: show it, change it, put it into effect and save it.
  const auto Flag = [this](std::string label, bool EngineConfig::*field) {
    AddToggle(SettingsPage::Graphics, std::move(label), [this, field] { return graphics.*field; }, [this, field](const bool value) {
      graphics.*field = value;
      ApplyGraphics();
      SaveGraphics(); });
  };
  AddChoice(SettingsPage::Graphics, "Display", {{"Windowed"}, {"Fullscreen"}}, [this] { return graphics.fullscreen ? size_t{1} : size_t{0}; }, [this](const size_t index) {
    graphics.fullscreen = index == 1;
    app.SetFullscreen(graphics.fullscreen);
    SaveGraphics(); });
  Flag("Vertical sync", &EngineConfig::vsync);
  static constexpr AntiAliasingMode MODES[]{AntiAliasingMode::None, AntiAliasingMode::MSAA2x, AntiAliasingMode::MSAA4x, AntiAliasingMode::MSAA8x};
  AddChoice(SettingsPage::Graphics, "Anti-aliasing", {{"Off"}, {"2x MSAA"}, {"4x MSAA"}, {"8x MSAA"}}, [this] { return static_cast<size_t>(std::ranges::find(MODES, graphics.antiAliasing) - std::begin(MODES)); }, [this](const size_t index) {
    graphics.antiAliasing = MODES[std::min(index, std::size(MODES) - 1)];
    ApplyGraphics();
    SaveGraphics(); });
  // Where the backend has a probe field to choose about; one without draws no bounce at any level.
  if (capabilities.probeVolume)
    AddChoice(SettingsPage::Graphics, "Lighting", {{"Low"}, {"Medium"}, {"High"}}, [this] { return static_cast<size_t>(graphics.lighting); }, [this](const size_t index) {
      graphics.lighting = static_cast<LightingQuality>(std::min<size_t>(index, 2));
      ApplyGraphics();
      SaveGraphics(); });
  Flag("Shadows", &EngineConfig::shadows);
  Flag("Bloom", &EngineConfig::bloom);
  if (capabilities.volumetrics)
    Flag("Fog and clouds", &EngineConfig::volumetrics);
  Flag("Reflections", &EngineConfig::reflections);
  Flag("Frame rate", &EngineConfig::showFrameRate);
}
auto SettingsMenu::ShowNotice(std::string text) -> void {
  notice = std::move(text);
  noticeLeft = NOTICE_LENGTH;
}
auto SettingsMenu::SwitchPage(const SettingsPage next) -> void {
  if (next == page)
    return;
  page = next;
  // The focus stays on the pages if it is there, so the arrows can go on along them; anywhere else
  // it goes to the top of the new page.
  if (focus != TAB_ROW)
    focus = 0;
  scroll = 0;
  capturing.clear();
  pendingModifier = 0;
}
auto SettingsMenu::CyclePage(const int delta) -> void {
  const auto pages = Pages();
  if (pages.empty())
    return;
  const auto count = static_cast<int>(pages.size());
  const auto at = static_cast<int>(std::ranges::find(pages, page) - pages.begin());
  SwitchPage(pages[static_cast<size_t>(((at + delta) % count + count) % count)]);
}
auto SettingsMenu::SaveBindings() -> void {
  auto &input = app.inputManager;
  auto &preferences = app.GetPreferences();
  for (const auto &name : input.GetBindingNames()) {
    const auto key = "input." + name;
    if (const auto trigger = input.GetBinding(name); trigger == input.GetDefaultBinding(name))
      preferences.Remove(key);
    else
      preferences.Set(key, FormatTrigger(trigger));
  }
  // Now as well as on the way out, so a rebinding survives a game that does not get to shut down.
  preferences.Save();
}
auto SettingsMenu::Capture(const InputManager::Trigger &trigger) -> void {
  auto &input = app.inputManager;
  const auto name = std::exchange(capturing, {});
  pendingModifier = 0;
  if (!name.empty())
    Assign(name, trigger);
}
auto SettingsMenu::Assign(const std::string &name, const InputManager::Trigger &trigger) -> void {
  auto &input = app.inputManager;
  const auto Label = [&](const std::string &binding) {
    const auto description = input.GetBindingDescription(binding);
    return description.empty() ? binding : description;
  };
  if (trigger.key != 0 && name != BINDING && trigger == input.GetBinding(BINDING)) {
    ShowNotice(std::format("{} opens these settings", input.GetTriggerName(trigger)));
    return;
  }
  // A key belongs to one binding, so whichever had it loses it; said here, where the player is
  // looking, as well as in the log where `InputManager` says it. No key at all is nobody's.
  std::string displaced;
  if (trigger.key != 0)
    for (const auto &other : input.GetBindingNames())
      if (other != name && input.GetBinding(other) == trigger)
        displaced = other;
  input.SetBinding(name, trigger);
  if (!displaced.empty())
    ShowNotice(std::format("{} is now unbound", Label(displaced)));
  SaveBindings();
}
auto SettingsMenu::Step(const Row &row, const int delta) -> void {
  if (row.kind != Row::Kind::Entry || row.entry >= entries.size())
    return;
  const auto &entry = entries[row.entry];
  if (entry.kind == EntryKind::Action)
    return;
  const auto count = static_cast<int>(entry.options.size());
  const auto current = static_cast<int>(std::min(entry.get(), entry.options.size() - 1));
  entry.set(static_cast<size_t>(((current + delta) % count + count) % count));
}
auto SettingsMenu::Activate(const int id, const std::vector<Row> &rows) -> void {
  if (id >= TAB_ID && id < TAB_ID + 3) {
    focus = TAB_ROW;
    SwitchPage(static_cast<SettingsPage>(id - TAB_ID));
    return;
  }
  if (id == RESUME_ID) {
    Close();
    return;
  }
  if (id == QUIT_ID) {
    app.Quit();
    return;
  }
  if (id < ROW_ID)
    return;
  const auto index = (id - ROW_ID) / 4;
  const auto part = (id - ROW_ID) % 4;
  if (index < 0 || index >= static_cast<int>(rows.size()))
    return;
  const auto &row = rows[static_cast<size_t>(index)];
  focus = index;
  if (part == PART_PREVIOUS || part == PART_NEXT) {
    Step(row, part == PART_NEXT ? 1 : -1);
    return;
  }
  switch (row.kind) {
  case Row::Kind::Entry:
    if (row.entry < entries.size() && entries[row.entry].kind == EntryKind::Action) {
      // Closed first, so that what the action does is seen happening rather than under the menu.
      const auto run = entries[row.entry].set;
      Close();
      run(0);
    } else
      Step(row, 1);
    return;
  case Row::Kind::Binding:
    capturing = row.binding;
    pendingModifier = 0;
    return;
  case Row::Kind::ResetBindings: {
    auto &input = app.inputManager;
    for (const auto &name : input.GetBindingNames())
      input.SetBinding(name, input.GetDefaultBinding(name));
    SaveBindings();
    ShowNotice("Every key is back where it started");
    return;
  }
  }
}
auto SettingsMenu::Handle(const Event &event, const std::vector<Row> &rows) -> void {
  const auto rowCount = static_cast<int>(rows.size());
  const auto FocusId = [&] {
    if (focus == TAB_ROW)
      return TAB_ID + static_cast<int>(page);
    if (focus < rowCount)
      return RowId(focus, PART_WHOLE);
    return focus == rowCount ? RESUME_ID : QUIT_ID;
  };
  // A key pressed says the keyboard is in use; a click or the wheel says the pointer is. A key let go
  // says nothing, since it was pressed first.
  if (event.kind == Event::Kind::Key)
    pointerLast = false;
  else if (event.kind != Event::Kind::KeyUp)
    pointerLast = true;
  switch (event.kind) {
  case Event::Kind::Key: {
    if (!capturing.empty()) {
      // Escape lets go of the binding as it was, as the editor's rebinding does; a modifier waits
      // to see whether it is being held for another key or is the key wanted.
      if (event.key == GLFW_KEY_ESCAPE) {
        capturing.clear();
        pendingModifier = 0;
      } else if (IsModifier(event.key))
        pendingModifier = event.key;
      else
        Capture({event.key, event.mods});
      return;
    }
    const auto settings = app.inputManager.GetBinding(BINDING);
    if (event.key == GLFW_KEY_ESCAPE || (settings.key != 0 && event.key == settings.key && (event.mods & settings.mods) == settings.mods)) {
      Close();
      return;
    }
    switch (event.key) {
    case GLFW_KEY_UP:
      // Up from the footer to the last row, and from the first row onto the pages.
      focus = focus > rowCount ? rowCount - 1 : std::max(TAB_ROW, focus - 1);
      revealFocus = true;
      return;
    case GLFW_KEY_DOWN:
      focus = std::min(focus + 1, rowCount);
      revealFocus = true;
      return;
    case GLFW_KEY_LEFT:
    case GLFW_KEY_RIGHT:
      if (focus == TAB_ROW)
        CyclePage(event.key == GLFW_KEY_RIGHT ? 1 : -1);
      else if (focus < rowCount)
        Step(rows[static_cast<size_t>(focus)], event.key == GLFW_KEY_RIGHT ? 1 : -1);
      else
        focus = event.key == GLFW_KEY_RIGHT ? rowCount + 1 : rowCount;
      return;
    case GLFW_KEY_ENTER:
    case GLFW_KEY_KP_ENTER:
    case GLFW_KEY_SPACE:
      Activate(FocusId(), rows);
      return;
    case GLFW_KEY_TAB:
      CyclePage((event.mods & GLFW_MOD_SHIFT) ? -1 : 1);
      return;
    // The keyboard's way to what the right click does, and to the one thing nothing else does: one
    // binding back to its default without resetting the rest. The settings binding cannot be
    // cleared, for the reason it cannot be given away; it can be restored.
    case GLFW_KEY_DELETE:
    case GLFW_KEY_BACKSPACE: {
      if (focus < 0 || focus >= rowCount)
        return;
      const auto &row = rows[static_cast<size_t>(focus)];
      if (row.kind != Row::Kind::Binding)
        return;
      if (event.key == GLFW_KEY_BACKSPACE)
        Assign(row.binding, app.inputManager.GetDefaultBinding(row.binding));
      else if (row.binding != BINDING)
        Assign(row.binding, {});
      return;
    }
    default:
      return;
    }
  }
  case Event::Kind::KeyUp:
    if (!capturing.empty() && pendingModifier != 0 && event.key == pendingModifier)
      Capture({event.key, 0});
    return;
  case Event::Kind::Click: {
    const auto id = app.PickOverlay(app.GetMousePosition());
    // A click anywhere but on the waiting binding lets go of it, which is the other way out of it
    // besides Escape. A click off the menu's own lines does nothing more than that.
    if (!capturing.empty()) {
      const auto index = (id - ROW_ID) / 4;
      const auto same = id >= ROW_ID && index < rowCount && rows[static_cast<size_t>(index)].binding == capturing;
      capturing.clear();
      pendingModifier = 0;
      if (same || id < ROW_ID)
        return;
    }
    Activate(id, rows);
    return;
  }
  case Event::Kind::RightClick: {
    // On a key, clears it; the settings binding is kept, since without it the menu is unreachable.
    const auto id = app.PickOverlay(app.GetMousePosition());
    const auto index = (id - ROW_ID) / 4;
    if (id < ROW_ID || index >= rowCount)
      return;
    const auto &row = rows[static_cast<size_t>(index)];
    if (row.kind != Row::Kind::Binding || row.binding == BINDING)
      return;
    capturing.clear();
    Assign(row.binding, {});
    return;
  }
  case Event::Kind::Scroll:
    scroll -= static_cast<int>(std::round(event.scroll));
    return;
  }
}
auto SettingsMenu::ValueText(const Row &row) const -> std::string {
  const auto &input = app.inputManager;
  switch (row.kind) {
  case Row::Kind::Entry: {
    const auto &entry = entries[row.entry];
    if (entry.kind == EntryKind::Action)
      return "";
    return entry.options[std::min(entry.get(), entry.options.size() - 1)].label;
  }
  case Row::Kind::Binding: {
    if (capturing == row.binding)
      return "Press a key";
    const auto trigger = input.GetBinding(row.binding);
    return trigger.key == 0 ? "Unbound" : input.GetTriggerName(trigger);
  }
  case Row::Kind::ResetBindings:
  default:
    return "";
  }
}
auto SettingsMenu::Update(const float deltaTime) -> void {
  noticeLeft = std::max(noticeLeft - deltaTime, .0f);
  auto &overlay = app.GetOverlay();
  const auto *context = app.graphicsContext.get();
  const auto [width, height] = context ? context->GetSurfaceSize() : std::pair{0, 0};
  const auto unit = static_cast<float>(height) / 1080.f;
  // One at a time, the rows built again after each: an event can change the page, or a value that
  // decides whether a row is shown at all.
  auto rows = open ? BuildRows() : std::vector<Row>{};
  for (size_t index = 0; index < events.size() && open; ++index) {
    Handle(events[index], rows);
    rows = BuildRows();
  }
  events.clear();
  if (!open) {
    if (graphics.showFrameRate && height > 0)
      if (const auto *rendering = app.GetRenderingSystem(); rendering)
        overlay.DrawText(std::format("{} fps", rendering->GetFPS()), {-16.f * unit, 14.f * unit}, 20.f * unit, {1.f, 1.f, 1.f, .6f}, TextAnchor::TopRight);
    return;
  }
  // Nothing else the overlay was given this frame is shown while the menu is: a game's bars and
  // panels, the network's status, the frame rate. The menu is drawn over the scene alone. Emptied
  // here rather than asked of each system, because the menu is queued after all of them and is the
  // one place that knows it is open; the overlay is immediate, so they come back the frame it shuts.
  overlay.Clear();
  const auto rowCount = static_cast<int>(rows.size());
  // The pointer takes the focus where it moves to, and only when it moves, so that a mouse left
  // resting over a row does not keep pulling the focus back from the arrow keys.
  if (const auto mouse = app.GetMousePosition(); mouse != lastMouse) {
    lastMouse = mouse;
    pointerLast = true;
    const auto id = app.PickOverlay(mouse);
    if (id >= ROW_ID && (id - ROW_ID) / 4 < rowCount)
      focus = (id - ROW_ID) / 4;
    else if (id >= TAB_ID && id < TAB_ID + 3)
      focus = TAB_ROW;
    else if (id == RESUME_ID)
      focus = rowCount;
    else if (id == QUIT_ID)
      focus = rowCount + 1;
  }
  focus = std::clamp(focus, TAB_ROW, rowCount + 1);
  // The pages are above the rows, so reaching them shows the rows from the top.
  if (revealFocus && focus == TAB_ROW)
    scroll = 0;
  else if (revealFocus && focus < rowCount) {
    if (focus < scroll)
      scroll = focus;
    else if (focus >= scroll + visibleRows)
      scroll = focus - visibleRows + 1;
  }
  revealFocus = false;
  Draw(rows);
}
auto SettingsMenu::Draw(const std::vector<Row> &rows) -> void {
  auto &overlay = app.GetOverlay();
  const auto &font = overlay.GetFont();
  const auto *context = app.graphicsContext.get();
  if (!font.IsLoaded() || !context)
    return;
  const auto [surfaceWidth, surfaceHeight] = context->GetSurfaceSize();
  if (surfaceWidth <= 0 || surfaceHeight <= 0)
    return;
  const auto width = static_cast<float>(surfaceWidth);
  const auto height = static_cast<float>(surfaceHeight);
  const auto unit = height / 1080.f;
  const auto rowCount = static_cast<int>(rows.size());
  // Text is placed by its baseline rather than by its ink, so that a line with a descender in it
  // sits level with one without; and measured by its ink, which is what the eye lines up.
  enum class Align : uint8_t { Left,
    Centre,
    Right };
  const auto Ink = [&](const std::string_view text, const float size) {
    glm::vec2 min, max;
    return font.Bounds(text, min, max) ? (max.x - min.x) * size : .0f;
  };
  const auto Text = [&](const std::string_view text, const float x, const float baseline, const float size, const glm::vec4 &color, const Align align, const int id = Overlay::NoHit) {
    glm::vec2 min, max;
    if (!font.Bounds(text, min, max))
      return;
    const auto top = baseline - max.y * size;
    switch (align) {
    case Align::Left:
      overlay.DrawText(std::string(text), {x, top}, size, color, TextAnchor::TopLeft, id);
      return;
    case Align::Centre:
      overlay.DrawText(std::string(text), {x - width * .5f, top}, size, color, TextAnchor::Top, id);
      return;
    case Align::Right:
      overlay.DrawText(std::string(text), {x - width, top}, size, color, TextAnchor::TopRight, id);
      return;
    }
  };
  const auto Rect = [&](const float x, const float y, const float w, const float h, const glm::vec4 &color, const int id = Overlay::NoHit) {
    overlay.DrawRect({x, y}, {w, h}, color, TextAnchor::TopLeft, id);
  };
  // Over everything the game drew, and answering every point of the window, so nothing under it can
  // be pointed at through it.
  Rect(.0f, .0f, width, height, BACKDROP, BACKDROP_ID);
  const auto panel = std::min(PANEL_WIDTH * unit, width - 48.f * unit);
  const auto left = (width - panel) * .5f;
  const auto right = left + panel;
  const auto centre = width * .5f;
  // From a fixed point near the top and downward, with as many rows as fit between there and the
  // bottom. Centred, the title and the pages moved whenever a page with a different number of rows
  // was shown; held at the top, only the footer moves, which is the end of the list it follows.
  const auto top = TOP_MARGIN * unit;
  const auto room = height / unit - TOP_MARGIN - HEADER_HEIGHT - FOOTER_HEIGHT - 24.f;
  visibleRows = std::max(3, static_cast<int>(room / ROW_HEIGHT));
  const auto shown = std::min(rowCount, visibleRows);
  scroll = std::clamp(scroll, 0, std::max(0, rowCount - visibleRows));
  Text("SETTINGS", centre, top + 40.f * unit, TITLE_SIZE * unit, TEXT, Align::Centre);
  // The pages, side by side under the title; the one shown is underlined.
  const auto pages = Pages();
  const auto gap = 56.f * unit;
  auto tabsWidth = .0f;
  for (const auto each : pages)
    tabsWidth += Ink(PageName(each), TAB_SIZE * unit);
  tabsWidth += gap * static_cast<float>(pages.size() - 1);
  auto x = centre - tabsWidth * .5f;
  const auto tabBaseline = top + 100.f * unit;
  const auto hovered = app.PickOverlay(app.GetMousePosition());
  // No band behind the pages while they have the focus, as a row has: the pages are a heading, not
  // a line of the list. The focus shows in the pages the arrows would move to, which brighten.
  const glm::vec4 offered = focus == TAB_ROW ? glm::vec4(1.f, 1.f, 1.f, .8f) : DIM;
  for (const auto each : pages) {
    const auto name = PageName(each);
    const auto w = Ink(name, TAB_SIZE * unit);
    const auto id = TAB_ID + static_cast<int>(each);
    Text(name, x, tabBaseline, TAB_SIZE * unit, each == page ? TEXT : hovered == id ? glm::vec4(1.f, 1.f, 1.f, .8f)
                                                                                    : offered,
      Align::Left,
      id);
    if (each == page)
      Rect(x, tabBaseline + 10.f * unit, w, 3.f * unit, TEXT);
    x += w + gap;
  }
  const auto rowsTop = top + HEADER_HEIGHT * unit;
  Rect(left, rowsTop - 12.f * unit, panel, 1.f * unit, RULE);
  for (auto line = 0; line < shown; ++line) {
    const auto index = scroll + line;
    const auto &row = rows[static_cast<size_t>(index)];
    const auto y = rowsTop + static_cast<float>(line) * ROW_HEIGHT * unit;
    const auto focused = index == focus;
    const auto baseline = y + (ROW_HEIGHT * .5f + ROW_SIZE * .36f) * unit;
    const auto size = ROW_SIZE * unit;
    // The whole line answers a click, and is lit while it has the focus.
    Rect(left, y, panel, (ROW_HEIGHT - 4.f) * unit, focused ? FOCUS : glm::vec4(.0f), RowId(index, PART_WHOLE));
    std::string label;
    auto choice = false;
    const std::vector<glm::vec4> *swatches = nullptr;
    switch (row.kind) {
    case Row::Kind::Entry: {
      const auto &entry = entries[row.entry];
      label = entry.label;
      choice = entry.kind == EntryKind::Choice;
      if (choice)
        swatches = &entry.options[std::min(entry.get(), entry.options.size() - 1)].swatches;
      break;
    }
    case Row::Kind::Binding: {
      const auto description = app.inputManager.GetBindingDescription(row.binding);
      label = description.empty() ? row.binding : description;
      break;
    }
    case Row::Kind::ResetBindings:
      label = "Reset to defaults";
      break;
    }
    const auto isAction = row.kind == Row::Kind::ResetBindings || (row.kind == Row::Kind::Entry && entries[row.entry].kind == EntryKind::Action);
    Text(label, left + SIDE * unit, baseline, size, focused ? TEXT : isAction ? glm::vec4(1.f, 1.f, 1.f, .75f)
                                                                              : glm::vec4(1.f, 1.f, 1.f, .82f),
      Align::Left);
    const auto value = ValueText(row);
    if (value.empty())
      continue;
    const auto waiting = row.kind == Row::Kind::Binding && capturing == row.binding;
    const auto unbound = row.kind == Row::Kind::Binding && !waiting && app.inputManager.GetBinding(row.binding).key == 0;
    auto edge = right - SIDE * unit;
    // A choice is flanked by arrows that step it either way; the line itself steps it forward.
    if (choice) {
      const auto arrow = focused ? TEXT : FAINT;
      Text(">", edge, baseline, size, arrow, Align::Right, RowId(index, PART_NEXT));
      edge -= Ink(">", size) + 18.f * unit;
    }
    Text(value, edge, baseline, size, waiting ? ACCENT : unbound ? FAINT
                                                       : focused ? TEXT
                                                                 : DIM,
      Align::Right);
    edge -= Ink(value, size);
    if (swatches && !swatches->empty()) {
      const auto swatch = size * .9f;
      edge -= 14.f * unit;
      for (auto it = swatches->rbegin(); it != swatches->rend(); ++it) {
        Rect(edge - swatch, baseline - swatch * .82f, swatch, swatch, *it);
        edge -= swatch;
      }
    }
    if (choice) {
      edge -= 18.f * unit;
      Text("<", edge, baseline, size, focused ? TEXT : FAINT, Align::Right, RowId(index, PART_PREVIOUS));
    }
  }
  // A thin bar down the right where the page has more rows than fit, saying how far down it is.
  if (rowCount > shown) {
    const auto track = static_cast<float>(shown) * ROW_HEIGHT * unit;
    Rect(right + 10.f * unit, rowsTop, 4.f * unit, track, RULE);
    Rect(right + 10.f * unit, rowsTop + track * static_cast<float>(scroll) / static_cast<float>(rowCount), 4.f * unit, track * static_cast<float>(shown) / static_cast<float>(rowCount), DIM);
  }
  const auto footerTop = rowsTop + static_cast<float>(shown) * ROW_HEIGHT * unit + 12.f * unit;
  Rect(left, footerTop, panel, 1.f * unit, RULE);
  // Resume and Quit stand as far below the lower rule as the pages stand above the upper one, from
  // the rule to the ink either way, so the list sits between two equal margins. Measured rather than
  // set, so it holds whatever the sizes become.
  const auto margin = rowsTop - 12.f * unit - tabBaseline;
  glm::vec2 footerMin, footerMax;
  const auto footerAscent = font.Bounds("RESUME", footerMin, footerMax) ? footerMax.y * FOOTER_SIZE * unit : FOOTER_SIZE * .7f * unit;
  const auto footerBaseline = footerTop + margin + footerAscent;
  // Under the footer, out of that margin: a notice if there is one, and on the controls page
  // otherwise the one thing about them that is not obvious from looking.
  const auto hintBaseline = footerBaseline + 48.f * unit;
  if (noticeLeft > .0f)
    Text(notice, centre, hintBaseline, NOTICE_SIZE * unit, {ACCENT.r, ACCENT.g, ACCENT.b, std::min(1.f, noticeLeft * 2.f)}, Align::Centre);
  else if (page == SettingsPage::Controls)
    Text(!capturing.empty() ? "Press the new key, or Escape to keep the old one" : pointerLast ? "Click a key to change it, right click to clear it"
                                                                                               : "Enter changes a key, Delete clears it, Backspace restores it",
      centre,
      hintBaseline,
      NOTICE_SIZE * unit,
      FAINT,
      Align::Centre);
  const auto footerGap = 90.f * unit;
  const auto resumeWidth = Ink("RESUME", FOOTER_SIZE * unit);
  const auto quitWidth = Ink("QUIT", FOOTER_SIZE * unit);
  const auto footerLeft = centre - (resumeWidth + footerGap + quitWidth) * .5f;
  Text("RESUME", footerLeft, footerBaseline, FOOTER_SIZE * unit, focus == rowCount ? TEXT : DIM, Align::Left, RESUME_ID);
  Text("QUIT", footerLeft + resumeWidth + footerGap, footerBaseline, FOOTER_SIZE * unit, focus == rowCount + 1 ? TEXT : DIM, Align::Left, QUIT_ID);
  if (focus >= rowCount) {
    const auto at = focus == rowCount ? footerLeft : footerLeft + resumeWidth + footerGap;
    Rect(at, footerBaseline + 10.f * unit, focus == rowCount ? resumeWidth : quitWidth, 3.f * unit, TEXT);
  }
}
} // namespace kuki
