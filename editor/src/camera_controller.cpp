#include <GLFW/glfw3.h>
#include <algorithm>
#include <application.hpp>
#include <application_internal.hpp>
#include <camera.hpp>
#include <camera_controller.hpp>
#include <camera_type.hpp>
#include <entity_manager.hpp>
#include <field.hpp>
#include <glm/detail/type_vec2.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/quaternion_float.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <id.hpp>
#include <imguizmo.hpp>
#include <string>
#include <transform.hpp>
#include <utility>
using namespace kuki;
namespace {
/// @brief Points a camera at an entity and backs off until its bounds fill the view. The editor's
/// Focus binding; was `Scene::AlignView`, which nothing but this ever called.
auto FrameEntity(Application &app, const EntityID cameraEntity, const EntityID id) -> void {
  constexpr auto ORIENTATION = glm::vec3{0.f};
  constexpr auto DISTANCE_FACTOR = 2.f;
  const auto [camera, place] = app.GetEntityComponent<Camera, Transform>(cameraEntity);
  if (!camera || !place)
    return;
  const auto [transform, world] = app.GetEntityComponent<Transform, WorldMatrix>(id);
  const auto bounds = Internal(app).GetEntityBounds(id);
  if (!bounds || !transform || !world)
    return;
  const auto framing = kuki::Frame(*camera, bounds.GetWorldBounds(world->matrix), transform->position, ORIENTATION, DISTANCE_FACTOR);
  place->position = framing.position;
  place->rotation = framing.rotation;
  camera->farPlane = framing.farPlane;
  app.MarkTransformDirty(cameraEntity);
}
struct BindingDef {
  const char *label;
  const char *description;
  int key;
  int mods;
};
enum BindingIndex : int {
  MoveForward,
  MoveBackward,
  MoveLeft,
  MoveRight,
  MoveUp,
  MoveDown,
  Boost,
  Precision,
  Focus,
  BindingCount
};
constexpr BindingDef kBindings[BindingCount] = {
  {"Move Forward", "Move the camera forward", GLFW_KEY_W, 0},
  {"Move Backward", "Move the camera backward", GLFW_KEY_S, 0},
  {"Move Left", "Move the camera left", GLFW_KEY_A, 0},
  {"Move Right", "Move the camera right", GLFW_KEY_D, 0},
  {"Move Up", "Move the camera up", GLFW_KEY_E, 0},
  {"Move Down", "Move the camera down", GLFW_KEY_Q, 0},
  {"Boost", "Hold to move the camera faster", GLFW_KEY_LEFT_SHIFT, 0},
  {"Precision", "Hold to move the camera slower, for fine adjustments", GLFW_KEY_LEFT_CONTROL, 0},
  {"Focus", "Focus the camera on the selected entity", GLFW_KEY_F, 0},
};
/// @brief What this controller's bindings are called in the one place bindings live.
///
/// Registered application-wide rather than held per controller, and so shared by every controller in
/// the scene. Two of them cannot be given different keys, which is a real limitation and the right
/// trade: the alternative is a set of keys per instance, which nothing can edit without a panel for
/// it.
///
/// What a shared registry does buy is that a collision is detectable. `InputManager::AssignBinding`
/// warns when one of these takes a key from something else, or when something else takes one of
/// these -- which is the failure worth catching, and it is caught in the engine, so a build with no
/// editor in it still reports two systems fighting over a key.
auto BindingName(int index) -> std::string {
  return std::string("CameraController.") + kBindings[index].label;
}
} // namespace
CameraController::CameraController(Application &app)
  : System(std::in_place_type<CameraController>, app) {}
auto CameraController::Display() -> void {
  field::DragFloat("Move Speed", &settings.moveSpeed, .1f, .1f, 100.f, "%.1f");
  field::DragFloat("Boost Multiplier", &settings.moveBoostMax, .1f, 1.f, 50.f, "%.1f");
  field::DragFloat("Precision Multiplier", &settings.precisionMin, .01f, .01f, 1.f, "%.2f");
  field::DragFloat("Boost Ramp Up Time", &settings.boostRampUpTime, .05f, .05f, 10.f, "%.2fs");
  field::DragFloat("Boost Ramp Down Time", &settings.boostRampDownTime, .05f, .05f, 10.f, "%.2fs");
}
auto CameraController::SetEntity(const EntityID id) -> void {
  entity = id;
}
auto CameraController::GetEntity() const -> EntityID {
  return entity;
}
auto CameraController::GetProjection() const -> glm::mat4 {
  const auto *camera = app.GetEntityComponent<Camera>(entity);
  return camera ? camera->transform.projection : glm::mat4(1.f);
}
auto CameraController::GetType() const -> CameraType {
  const auto *camera = app.GetEntityComponent<Camera>(entity);
  return camera ? camera->type : CameraType::Perspective;
}
auto CameraController::GetView() const -> glm::mat4 {
  const auto *camera = app.GetEntityComponent<Camera>(entity);
  return camera ? camera->transform.view : glm::mat4(1.f);
}
auto CameraController::Forward() const -> glm::vec3 {
  return -glm::mat3_cast(rotation)[2];
}
auto CameraController::Right() const -> glm::vec3 {
  return glm::mat3_cast(rotation)[0];
}
auto CameraController::Up() const -> glm::vec3 {
  return glm::mat3_cast(rotation)[1];
}
auto CameraController::Start() -> void {
  for (auto i = 0; i < BindingCount; ++i)
    Internal(app).RegisterBinding(BindingName(i), InputManager::Trigger{kBindings[i].key, kBindings[i].mods}, kBindings[i].description);
  rmbPressActionId = Internal(app).Input().RegisterAction(GLFW_MOUSE_BUTTON_RIGHT, [this]() {
    if (!Internal(app).IsViewportHovered())
      return;
    mouselook = true;
    mouseEnter = true;
    Internal(app).SetCursorLocked(true);
  });
  rmbReleaseActionId = Internal(app).Input().RegisterAction(
    GLFW_MOUSE_BUTTON_RIGHT,
    [this]() {
      mouselook = false;
      Internal(app).SetCursorLocked(false);
      const auto pos = app.GetMousePosition();
      ImGui::GetIO().AddMousePosEvent(pos.x, pos.y);
    },
    false);
  lmbPressActionId = Internal(app).Input().RegisterAction(GLFW_MOUSE_BUTTON_LEFT, [this]() {
    if (!Internal(app).IsViewportHovered() || ImGuizmo::IsOver() || ImGuizmo::IsUsing())
      return;
    if (const auto world = app.GetEntityComponent<WorldMatrix>(Internal(app).GetSelectedEntity()); world)
      orbitPivot = glm::vec3(world->matrix[3]);
    else
      orbitPivot = position + Forward() * settings.orbitFallbackDistance;
    orbitDistance = glm::length(position - orbitPivot);
    orbiting = true;
    mouseEnter = true;
    Internal(app).SetCursorLocked(true);
  });
  lmbReleaseActionId = Internal(app).Input().RegisterAction(
    GLFW_MOUSE_BUTTON_LEFT,
    [this]() {
      orbiting = false;
      Internal(app).SetCursorLocked(false);
      const auto pos = app.GetMousePosition();
      ImGui::GetIO().AddMousePosEvent(pos.x, pos.y);
    },
    false);
  mmbPressActionId = Internal(app).Input().RegisterAction(GLFW_MOUSE_BUTTON_MIDDLE, [this]() {
    if (!Internal(app).IsViewportHovered())
      return;
    panning = true;
    mouseEnter = true;
    Internal(app).SetCursorLocked(true);
  });
  mmbReleaseActionId = Internal(app).Input().RegisterAction(
    GLFW_MOUSE_BUTTON_MIDDLE,
    [this]() {
      panning = false;
      Internal(app).SetCursorLocked(false);
      const auto pos = app.GetMousePosition();
      ImGui::GetIO().AddMousePosEvent(pos.x, pos.y);
    },
    false);
}
auto CameraController::Shutdown() -> void {
  for (const auto action : {rmbPressActionId, rmbReleaseActionId, lmbPressActionId, lmbReleaseActionId, mmbPressActionId, mmbReleaseActionId})
    if (action != InputManager::InvalidActionID)
      Internal(app).Input().UnregisterAction(action);
}
auto CameraController::Update(const float) -> void {
  const auto [transform, camera] = app.GetEntityComponent<Transform, Camera>(entity);
  if (!transform || !camera)
    return;
  position = transform->position;
  rotation = transform->rotation;
  auto moved = false;
  moved |= UpdatePosition(app);
  moved |= mouselook && UpdateRotation(app);
  moved |= orbiting && UpdateOrbit(app);
  moved |= panning && UpdatePan(app);
  moved |= UpdateZoom(app, *camera);
  if (moved) {
    transform->position = position;
    transform->rotation = rotation;
    app.MarkTransformDirty(entity);
  }
  // After the input's move is written, which this replaces.
  if (app.IsBindingPressed(BindingName(Focus)))
    FrameEntity(app, entity, Internal(app).GetSelectedEntity());
}
auto CameraController::UpdateOrbit(Application &app) -> bool {
  static constexpr auto EPSILON = 1e-6f;
  static constexpr auto MAX_PITCH = glm::radians(89.f);
  static constexpr auto WORLD_UP = glm::vec3(0.f, 1.f, 0.f);
  static constexpr auto WORLD_RIGHT = glm::vec3(1.f, 0.f, 0.f);
  const auto mousePos = app.GetMousePosition();
  if (mouseEnter) {
    mouseLast = mousePos;
    const auto forward = Forward();
    pitch = glm::asin(std::clamp(forward.y, -1.f, 1.f));
    yaw = std::atan2(-forward.x, -forward.z);
  }
  mouseEnter = false;
  glm::vec2 mouseDiff{};
  mouseDiff.x = (mousePos.x - mouseLast.x) * settings.mouseSensitivity;
  mouseDiff.y = (mouseLast.y - mousePos.y) * settings.mouseSensitivity;
  mouseLast = mousePos;
  if (glm::length2(mouseDiff) < EPSILON)
    return false;
  yaw -= mouseDiff.x;
  pitch = std::clamp(pitch + mouseDiff.y, -MAX_PITCH, MAX_PITCH);
  rotation = glm::normalize(glm::angleAxis(yaw, WORLD_UP) * glm::angleAxis(pitch, WORLD_RIGHT));
  position = orbitPivot - Forward() * orbitDistance;
  return true;
}
auto CameraController::UpdatePan(Application &app) -> bool {
  static constexpr auto EPSILON = 1e-6f;
  const auto mousePos = app.GetMousePosition();
  if (mouseEnter) {
    mouseLast = mousePos;
    mouseEnter = false;
  }
  glm::vec2 mouseDiff{};
  mouseDiff.x = mousePos.x - mouseLast.x;
  mouseDiff.y = mousePos.y - mouseLast.y;
  mouseLast = mousePos;
  if (glm::length2(mouseDiff) < EPSILON)
    return false;
  position += (-Right() * mouseDiff.x + Up() * mouseDiff.y) * settings.panSpeed;
  return true;
}
auto CameraController::UpdatePosition(Application &app) -> bool {
  constexpr auto WORLD_UP = glm::vec3(.0f, 1.f, .0f);
  if (!Internal(app).IsViewportHovered() && !mouselook)
    return false;
  if (Internal(app).Input().IsSequenceInProgress())
    return false;
  const auto deltaTime = app.DeltaTime();
  const auto boost = app.IsBindingHeld(BindingName(Boost));
  if (boost)
    settings.boostTime = std::min(settings.boostTime + deltaTime, settings.boostRampUpTime);
  else
    settings.boostTime = std::max(0.f, settings.boostTime - deltaTime * (settings.boostRampUpTime / settings.boostRampDownTime));
  const auto precision = app.IsBindingHeld(BindingName(Precision));
  if (precision)
    settings.precisionTime = std::min(settings.precisionTime + deltaTime, settings.boostRampUpTime);
  else
    settings.precisionTime = std::max(0.f, settings.precisionTime - deltaTime * (settings.boostRampUpTime / settings.boostRampDownTime));
  const auto t = settings.boostRampUpTime > 0.f ? settings.boostTime / settings.boostRampUpTime : 1.f;
  settings.moveBoost = 1.f + (settings.moveBoostMax - 1.f) * (t * t * (3.f - 2.f * t));
  const auto pt = settings.boostRampUpTime > 0.f ? settings.precisionTime / settings.boostRampUpTime : 1.f;
  const auto precisionFactor = 1.f + (settings.precisionMin - 1.f) * (pt * pt * (3.f - 2.f * pt));
  glm::ivec2 wasd{};
  wasd.y = app.IsBindingHeld(BindingName(MoveForward)) - app.IsBindingHeld(BindingName(MoveBackward));
  wasd.x = app.IsBindingHeld(BindingName(MoveRight)) - app.IsBindingHeld(BindingName(MoveLeft));
  const auto eq = app.IsBindingHeld(BindingName(MoveUp)) - app.IsBindingHeld(BindingName(MoveDown));
  if (wasd.x == 0 && wasd.y == 0 && eq == 0)
    return false;
  const auto velocity = settings.moveSpeed * settings.moveBoost * precisionFactor * deltaTime;
  position += (Forward() * static_cast<float>(wasd.y) + Right() * static_cast<float>(wasd.x) + WORLD_UP * static_cast<float>(eq)) * velocity;
  return true;
}
auto CameraController::UpdateRotation(Application &app) -> bool {
  static constexpr auto EPSILON = 1e-6f;
  static constexpr auto MAX_PITCH = glm::radians(89.f);
  static constexpr auto WORLD_UP = glm::vec3(0.f, 1.f, 0.f);
  static constexpr auto WORLD_RIGHT = glm::vec3(1.f, 0.f, 0.f);
  const auto mousePos = app.GetMousePosition();
  if (mouseEnter) {
    mouseLast = mousePos;
    const auto forward = Forward();
    pitch = glm::asin(std::clamp(forward.y, -1.f, 1.f));
    yaw = std::atan2(-forward.x, -forward.z);
  }
  mouseEnter = false;
  glm::vec2 mouseDiff{};
  mouseDiff.x = (mousePos.x - mouseLast.x) * settings.mouseSensitivity;
  mouseDiff.y = (mouseLast.y - mousePos.y) * settings.mouseSensitivity;
  mouseLast = mousePos;
  if (glm::length2(mouseDiff) < EPSILON)
    return false;
  yaw -= mouseDiff.x;
  pitch = std::clamp(pitch + mouseDiff.y, -MAX_PITCH, MAX_PITCH);
  rotation = glm::normalize(glm::angleAxis(yaw, WORLD_UP) * glm::angleAxis(pitch, WORLD_RIGHT));
  return true;
}
auto CameraController::UpdateZoom(Application &app, Camera &camera) -> bool {
  static constexpr auto EPSILON = 1e-6f;
  static constexpr auto MIN_ORTHO_SIZE = .1f;
  if (!Internal(app).IsViewportHovered() && !mouselook)
    return false;
  const auto scroll = app.GetScrollOffset().y;
  if (glm::abs(scroll) < EPSILON)
    return false;
  auto zoomSpeed = settings.zoomSpeed;
  if (app.IsBindingHeld(BindingName(Precision)))
    zoomSpeed *= settings.precisionMin;
  else if (app.IsBindingHeld(BindingName(Boost)))
    zoomSpeed *= settings.moveBoostMax;
  if (camera.type == CameraType::Orthographic) {
    camera.orthoSize = std::max(MIN_ORTHO_SIZE, camera.orthoSize - scroll * zoomSpeed);
    return false;
  }
  position += Forward() * scroll * zoomSpeed;
  return true;
}
