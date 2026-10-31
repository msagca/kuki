#pragma once
#include <cstddef>
#include <string>
namespace kuki {
class Application;
} // namespace kuki
namespace frontier {
/// @brief The names of the frontier's bindings. Each is rebindable in the settings menu's controls,
/// which is where its keys are listed now instead of along the bottom of the picture.
///
/// The camera and the buildings share the turn keys and the modifier, as they always did: with a
/// building in hand the keys turn it rather than the view. That is one binding read by two systems
/// rather than two bindings, because a key belongs to one binding and two on the same key would take
/// it from each other.
inline constexpr const char *FORWARD = "Frontier.Forward";
inline constexpr const char *BACK = "Frontier.Back";
inline constexpr const char *LEFT = "Frontier.Left";
inline constexpr const char *RIGHT = "Frontier.Right";
inline constexpr const char *TURN_LEFT = "Frontier.TurnLeft";
inline constexpr const char *TURN_RIGHT = "Frontier.TurnRight";
inline constexpr const char *MODIFIER = "Frontier.Modifier";
inline constexpr const char *DEMOLISH = "Frontier.Demolish";
inline constexpr const char *LAYER = "Frontier.Layer";
inline constexpr const char *NEW_MAP = "Frontier.NewMap";
inline constexpr const char *TIME = "Frontier.Time";
/// @brief How many buildings have a key of their own, one to nine down the number row.
inline constexpr size_t BUILD_KEYS = 9;
/// @brief The binding that picks up the building at a place in the catalogue.
auto BuildBinding(const size_t) -> std::string;
/// @brief Registers every binding above, and one per building, in the order the controls list them.
///
/// Once, from the game's setup, before any system starts reading them: the list is the game's, not
/// any one system's, and its order is the order a player reads it in.
auto RegisterControls(kuki::Application &) -> void;
} // namespace frontier
