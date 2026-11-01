#include <algorithm>
#include <application.hpp>
#include <controls.hpp>
#include <format>
#include <key.hpp>
#include <props.hpp>
#include <string>
#include <utility>
namespace frontier {
auto BuildBinding(const size_t index) -> std::string {
  return std::format("Frontier.Build{}", index + 1);
}
auto RegisterControls(kuki::Application &app) -> void {
  using kuki::Key;
  // The arrow keys used to move the camera as well as these. A binding has one key, so they are no
  // longer a second way of doing it; any of the four can be moved onto them here.
  app.RegisterBinding(FORWARD, Key::W, "Move forward");
  app.RegisterBinding(BACK, Key::S, "Move back");
  app.RegisterBinding(LEFT, Key::A, "Move left");
  app.RegisterBinding(RIGHT, Key::D, "Move right");
  app.RegisterBinding(TURN_LEFT, Key::Q, "Turn left");
  app.RegisterBinding(TURN_RIGHT, Key::E, "Turn right");
  app.RegisterBinding(MODIFIER, Key::LeftShift, "Hold to move faster, or turn a building finely");
  const auto &types = Buildings(app);
  for (size_t i = 0; i < std::min(types.size(), BUILD_KEYS); ++i)
    app.RegisterBinding(BuildBinding(i), static_cast<Key>(std::to_underlying(Key::Digit1) + i), std::format("Build {}", types[i].name));
  app.RegisterBinding(DEMOLISH, Key::X, "Pull down the building under the cursor");
  app.RegisterBinding(LAYER, Key::Tab, "Next layer");
  app.RegisterBinding(NEW_MAP, Key::G, "New map");
  app.RegisterBinding(TIME, Key::T, "Let the day pass, or stop it");
}
} // namespace frontier
