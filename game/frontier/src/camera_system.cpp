#include <GLFW/glfw3.h>
#include <algorithm>
#include <application.hpp>
#include <camera.hpp>
#include <camera_system.hpp>
#include <cmath>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/matrix.hpp>
#include <scene.hpp>
using namespace kuki;
namespace {
constexpr float NEAREST = 6.f;
constexpr float FURTHEST = 70.f;
/// How quickly each value closes on where it is going, per second.
constexpr float EASE = 9.f;
/// How far down the camera looks, in radians, close in and far out.
constexpr float FLATTEST = .55f;
constexpr float STEEPEST = 1.1f;
} // namespace
CameraSystem::CameraSystem(Application &app)
  : System(std::in_place_type<CameraSystem>, app) {}
auto CameraSystem::PostStart() -> void {
  if (const auto value = GetFlagValue(GetCommandLineArguments(), DISTANCE_FLAG)) {
    try {
      targetDistance = distance = std::clamp(std::stof(*value), NEAREST, FURTHEST);
    } catch (...) {
    }
  }
}
auto CameraSystem::SetBounds(const glm::vec2 &low, const glm::vec2 &high) -> void {
  min = low;
  max = high;
  target = focus = (low + high) * .5f;
}
auto CameraSystem::LookAt(const glm::vec2 &point) -> void {
  target = focus = glm::clamp(point, min, max);
}
auto CameraSystem::SetTurnKeys(const bool enabled) -> void {
  turnKeys = enabled;
}
auto CameraSystem::SetFocusHeight(const float value) -> void {
  targetHeight = std::max(value, .0f);
}
auto CameraSystem::GetFocus() const -> glm::vec3 {
  return {focus.x, height, focus.y};
}
auto CameraSystem::GetYaw() const -> float {
  return yaw;
}
auto CameraSystem::GetDistance() const -> float {
  return distance;
}
auto CameraSystem::Update(const float deltaTime) -> void {
  auto scene = app.GetScene();
  auto *camera = scene ? scene->GetCamera() : nullptr;
  if (!camera)
    return;
  const auto dt = std::min(deltaTime, .1f);
  // Along the ground, relative to which way the camera faces: forward is away from it.
  const glm::vec2 forward{-std::sin(yaw), -std::cos(yaw)};
  const glm::vec2 right{std::cos(yaw), -std::sin(yaw)};
  glm::vec2 move{};
  if (app.IsInputHeld(GLFW_KEY_W) || app.IsInputHeld(GLFW_KEY_UP))
    move += forward;
  if (app.IsInputHeld(GLFW_KEY_S) || app.IsInputHeld(GLFW_KEY_DOWN))
    move -= forward;
  if (app.IsInputHeld(GLFW_KEY_D) || app.IsInputHeld(GLFW_KEY_RIGHT))
    move += right;
  if (app.IsInputHeld(GLFW_KEY_A) || app.IsInputHeld(GLFW_KEY_LEFT))
    move -= right;
  const auto fast = app.IsInputHeld(GLFW_KEY_LEFT_SHIFT) ? 2.5f : 1.f;
  if (glm::length(move) > .0f)
    target += glm::normalize(move) * targetDistance * .9f * fast * dt;
  target = glm::clamp(target, min, max);
  if (turnKeys) {
    if (app.IsInputHeld(GLFW_KEY_Q))
      targetYaw -= 1.6f * dt;
    if (app.IsInputHeld(GLFW_KEY_E))
      targetYaw += 1.6f * dt;
  }
  if (const auto scroll = app.GetScrollOffset().y; scroll != .0f)
    targetDistance = std::clamp(targetDistance * std::pow(.88f, scroll), NEAREST, FURTHEST);
  const auto cursor = app.GetMousePosition();
  if (app.IsInputPressed(GLFW_MOUSE_BUTTON_MIDDLE)) {
    dragging = true;
    dragFrom = cursor;
  }
  if (dragging) {
    if (!app.IsInputHeld(GLFW_MOUSE_BUTTON_MIDDLE))
      dragging = false;
    else {
      const auto delta = cursor - dragFrom;
      dragFrom = cursor;
      targetYaw -= delta.x * .006f;
      targetTilt = std::clamp(targetTilt + delta.y * .004f, -.35f, .35f);
    }
  }
  const auto ease = 1.f - std::exp(-EASE * dt);
  focus += (target - focus) * ease;
  yaw += (targetYaw - yaw) * ease;
  distance += (targetDistance - distance) * ease;
  tilt += (targetTilt - tilt) * ease;
  height += (targetHeight - height) * (1.f - std::exp(-3.f * dt));
  const auto zoom = (distance - NEAREST) / (FURTHEST - NEAREST);
  const auto pitch = std::clamp(FLATTEST + (STEEPEST - FLATTEST) * std::sqrt(zoom) + tilt, .25f, 1.45f);
  const glm::vec3 centre{focus.x, height, focus.y};
  const glm::vec3 back{std::sin(yaw) * std::cos(pitch), std::sin(pitch), std::cos(yaw) * std::cos(pitch)};
  camera->position = centre + back * distance;
  camera->rotation = glm::quatLookAt(-back, glm::vec3(.0f, 1.f, .0f));
  ++camera->dirty;
}
auto CameraSystem::CursorRay() const -> std::pair<glm::vec3, glm::vec3> {
  auto scene = app.GetScene();
  const auto *camera = scene ? scene->GetCamera() : nullptr;
  const auto window = app.GetWindowSize();
  if (!camera || window.x <= 0 || window.y <= 0)
    return {{}, {.0f, -1.f, .0f}};
  const auto cursor = app.GetMousePosition();
  const glm::vec2 ndc{cursor.x / static_cast<float>(window.x) * 2.f - 1.f, 1.f - cursor.y / static_cast<float>(window.y) * 2.f};
  const auto inverse = glm::inverse(camera->transform.projection * camera->transform.view);
  // Two depths that are inside the frustum under either clip convention, so the line through them
  // is the cursor's whichever the backend projects with.
  auto near = inverse * glm::vec4(ndc, .1f, 1.f);
  auto far = inverse * glm::vec4(ndc, .9f, 1.f);
  near /= near.w;
  far /= far.w;
  return {camera->position, glm::normalize(glm::vec3(far - near))};
}
