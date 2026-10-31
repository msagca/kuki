#include <GLFW/glfw3.h>
#include <algorithm>
#include <application.hpp>
#include <controls.hpp>
#include <format>
#include <input_manager.hpp>
#include <props.hpp>
#include <string>
namespace frontier {
auto BuildBinding(const size_t index) -> std::string {
  return std::format("Frontier.Build{}", index + 1);
}
auto RegisterControls(kuki::Application &app) -> void {
  using Trigger = kuki::InputManager::Trigger;
  // The arrow keys used to move the camera as well as these. A binding has one key, so they are no
  // longer a second way of doing it; any of the four can be moved onto them here.
  app.RegisterBinding(FORWARD, Trigger{GLFW_KEY_W, 0}, "Move forward");
  app.RegisterBinding(BACK, Trigger{GLFW_KEY_S, 0}, "Move back");
  app.RegisterBinding(LEFT, Trigger{GLFW_KEY_A, 0}, "Move left");
  app.RegisterBinding(RIGHT, Trigger{GLFW_KEY_D, 0}, "Move right");
  app.RegisterBinding(TURN_LEFT, Trigger{GLFW_KEY_Q, 0}, "Turn left");
  app.RegisterBinding(TURN_RIGHT, Trigger{GLFW_KEY_E, 0}, "Turn right");
  app.RegisterBinding(MODIFIER, Trigger{GLFW_KEY_LEFT_SHIFT, 0}, "Hold to move faster, or turn a building finely");
  const auto &types = Buildings(app);
  for (size_t i = 0; i < std::min(types.size(), BUILD_KEYS); ++i)
    app.RegisterBinding(BuildBinding(i), Trigger{GLFW_KEY_1 + static_cast<int>(i), 0}, std::format("Build {}", types[i].name));
  app.RegisterBinding(DEMOLISH, Trigger{GLFW_KEY_X, 0}, "Pull down the building under the cursor");
  app.RegisterBinding(LAYER, Trigger{GLFW_KEY_TAB, 0}, "Next layer");
  app.RegisterBinding(NEW_MAP, Trigger{GLFW_KEY_G, 0}, "New map");
  app.RegisterBinding(TIME, Trigger{GLFW_KEY_T, 0}, "Let the day pass, or stop it");
}
} // namespace frontier
