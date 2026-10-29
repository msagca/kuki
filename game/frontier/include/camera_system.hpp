#pragma once
#include <array>
#include <command_line.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <system.hpp>
#include <utility>
namespace kuki {
class Application;
} // namespace kuki
/// @brief The camera a strategy game is looked at through: over the map, tilted down at it, turned
/// and zoomed about the point it is looking at.
///
/// WASD or the arrows move the point over the map, the wheel brings the camera nearer or further,
/// Q and E turn it about the point -- unless something else has the keys, as placing a building
/// does to turn the building -- and dragging with the middle button turns and tilts it. The nearer
/// it is, the flatter it looks, so that close in the land is seen across and far out it is seen
/// as a map.
///
/// Every value is eased towards where the keys put it, so the camera glides rather than steps.
class CameraSystem final : public kuki::System {
public:
  static constexpr kuki::Flag DISTANCE_FLAG{"--distance", kuki::FlagKind::Value, "<units>", "how far back the camera starts from the middle of the map, from 6 to 70"};
  static constexpr std::array FLAGS{DISTANCE_FLAG};
  explicit CameraSystem(kuki::Application &);
  auto PostStart() -> void override;
  auto Update(const float) -> void override;
  /// @brief Keeps the point it looks at over a rectangle of the map's plane, and starts it at the middle.
  auto SetBounds(const glm::vec2 &min, const glm::vec2 &max) -> void;
  /// @brief Moves the point it looks at there at once, without gliding.
  auto LookAt(const glm::vec2 &) -> void;
  /// @brief Lets Q and E turn the camera, or leaves them to something else.
  auto SetTurnKeys(const bool) -> void;
  /// @brief The line through the cursor into the scene: where it starts and which way it goes.
  auto CursorRay() const -> std::pair<glm::vec3, glm::vec3>;
  /// @brief The point it is looking at, and which way along the ground its right is.
  auto GetFocus() const -> glm::vec3;
  auto GetYaw() const -> float;
  auto GetDistance() const -> float;
  /// @brief Tells it how high the ground is under the point it looks at, so it rises over a hill.
  auto SetFocusHeight(const float) -> void;
private:
  glm::vec2 min{};
  glm::vec2 max{100.f};
  glm::vec2 target{};
  glm::vec2 focus{};
  float targetYaw{.6f};
  float yaw{.6f};
  float targetDistance{34.f};
  float distance{34.f};
  float tilt{};
  float targetTilt{};
  float height{};
  float targetHeight{};
  bool turnKeys{true};
  bool dragging{};
  glm::vec2 dragFrom{};
};
