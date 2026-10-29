#pragma once
#include <ambience_system.hpp>
#include <application_description.hpp>
#include <camera_system.hpp>
#include <flock_system.hpp>
#include <frontier_world.hpp>
#include <map_system.hpp>
#include <placement_system.hpp>
#include <rendering_system.hpp>
#include <system_application.hpp>
inline constexpr const char *FRONTIER_FONT = "font/Inter-VariableFont_opsz,wght.ttf";
namespace kuki {
class Application;
} // namespace kuki
/// @brief Loads the game's assets and builds the parts of its scene that every map shares: the
/// camera, the sun, the sky, the air, the wind and the look. The map itself is `MapSystem`'s.
auto SetupFrontier(kuki::Application &) -> void;
/// @brief A strategy map: an island grown on an irregular grid and filled in by a wave function
/// collapse, with layers for what can be gathered, walked and built on, and buildings to put down.
///
/// There are no units yet. The map, its layers and the navigation mesh under them are the ground a
/// strategy game stands on, and they are what this is here to show: the engine's grid, collapse,
/// navigation and flocking at work together, under the cel look with the wind in the grass and
/// waves on the sea.
///
/// The map comes first, so the cell under the cursor is known to everything after it; the camera
/// after it, so the map has the camera's last frame to cast the cursor's ray from; then the
/// buildings, the birds, and the weather, which writes the wind everything else is drawn in.
class Frontier final : public kuki::SystemApplication<frontier::FrontierWorld, MapSystem, CameraSystem, PlacementSystem, FlockSystem, AmbienceSystem, kuki::RenderingSystem> {
public:
  Frontier();
private:
  auto Start() -> void override;
};
