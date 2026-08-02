#define GLM_ENABLE_EXPERIMENTAL
#include <GLFW/glfw3.h>
#include <algorithm>
#include <application.hpp>
#include <camera.hpp>
#include <camera_controller.hpp>
#include <camera_type.hpp>
#include <entity_manager.hpp>
#include <glm/detail/type_vec2.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/quaternion_float.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <id.hpp>
#include <imguizmo.hpp>
#include <script.hpp>
#include <script_registry.hpp>
#include <string>
#include <transform.hpp>
#include <utility>
using namespace kuki;
namespace {
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
auto BindingName(int index) -> std::string {
  return std::string("CameraController.") + kBindings[index].label;
}
} // namespace
KUKI_REGISTER_SCRIPT(CameraController)
CameraController::CameraController()
  : Script(std::in_place_type<CameraController>) {}
CameraController::~CameraController() {
  if (!app)
    return;
  if (rmbPressActionId != InputManager::InvalidActionID)
    app->UnregisterInputAction(rmbPressActionId);
  if (rmbReleaseActionId != InputManager::InvalidActionID)
    app->UnregisterInputAction(rmbReleaseActionId);
  if (lmbPressActionId != InputManager::InvalidActionID)
    app->UnregisterInputAction(lmbPressActionId);
  if (lmbReleaseActionId != InputManager::InvalidActionID)
    app->UnregisterInputAction(lmbReleaseActionId);
  if (mmbPressActionId != InputManager::InvalidActionID)
    app->UnregisterInputAction(mmbPressActionId);
  if (mmbReleaseActionId != InputManager::InvalidActionID)
    app->UnregisterInputAction(mmbReleaseActionId);
}
auto CameraController::CloneTo(EntityManager &entityManager, const EntityID id) const -> void {
  auto script = entityManager.AddComponent<CameraController>(id);
  *script = *this;
  script->app = nullptr;
  script->rmbPressActionId = InputManager::InvalidActionID;
  script->rmbReleaseActionId = InputManager::InvalidActionID;
  script->lmbPressActionId = InputManager::InvalidActionID;
  script->lmbReleaseActionId = InputManager::InvalidActionID;
  script->mmbPressActionId = InputManager::InvalidActionID;
  script->mmbReleaseActionId = InputManager::InvalidActionID;
}
auto CameraController::Display() const -> void {
  ImGui::DragFloat("Move Speed", &settings.moveSpeed, .1f, .1f, 100.f, "%.1f");
  ImGui::DragFloat("Boost Multiplier", &settings.moveBoostMax, .1f, 1.f, 50.f, "%.1f");
  ImGui::DragFloat("Precision Multiplier", &settings.precisionMin, .01f, .01f, 1.f, "%.2f");
  ImGui::DragFloat("Boost Ramp Up Time", &settings.boostRampUpTime, .05f, .05f, 10.f, "%.2fs");
  ImGui::DragFloat("Boost Ramp Down Time", &settings.boostRampDownTime, .05f, .05f, 10.f, "%.2fs");
}
auto CameraController::GetName() const -> std::string {
  return "Camera Controller";
}
auto CameraController::GetProjection() const -> const glm::mat4 & {
  return camera.transform.projection;
}
auto CameraController::GetType() const -> const CameraType & {
  return camera.type;
}
auto CameraController::GetView() const -> const glm::mat4 & {
  return camera.transform.view;
}
auto CameraController::Start(Application &app) -> void {
  this->app = &app;
  for (auto i = 0; i < BindingCount; ++i)
    app.RegisterBinding(BindingName(i), InputManager::Trigger{kBindings[i].key, kBindings[i].mods}, kBindings[i].description);
  rmbPressActionId = app.RegisterInputAction(GLFW_MOUSE_BUTTON_RIGHT, [this, &app]() {
    if (!app.IsViewportHovered())
      return;
    mouselook = true;
    mouseEnter = true;
    app.SetCursorLocked(true);
  });
  rmbReleaseActionId = app.RegisterInputAction(
    GLFW_MOUSE_BUTTON_RIGHT,
    [this, &app]() {
      mouselook = false;
      app.SetCursorLocked(false);
      const auto pos = app.GetMousePosition();
      ImGui::GetIO().AddMousePosEvent(pos.x, pos.y);
    },
    false);
  lmbPressActionId = app.RegisterInputAction(GLFW_MOUSE_BUTTON_LEFT, [this, &app]() {
    if (!app.IsViewportHovered() || ImGuizmo::IsOver() || ImGuizmo::IsUsing())
      return;
    if (const auto transform = app.GetEntityComponent<Transform>(app.GetSelectedEntity()); transform)
      orbitPivot = glm::vec3(transform->world[3]);
    else
      orbitPivot = camera.position + camera.forward * settings.orbitFallbackDistance;
    orbitDistance = glm::length(camera.position - orbitPivot);
    orbiting = true;
    mouseEnter = true;
    app.SetCursorLocked(true);
  });
  lmbReleaseActionId = app.RegisterInputAction(
    GLFW_MOUSE_BUTTON_LEFT,
    [this, &app]() {
      orbiting = false;
      app.SetCursorLocked(false);
      const auto pos = app.GetMousePosition();
      ImGui::GetIO().AddMousePosEvent(pos.x, pos.y);
    },
    false);
  mmbPressActionId = app.RegisterInputAction(GLFW_MOUSE_BUTTON_MIDDLE, [this, &app]() {
    if (!app.IsViewportHovered())
      return;
    panning = true;
    mouseEnter = true;
    app.SetCursorLocked(true);
  });
  mmbReleaseActionId = app.RegisterInputAction(
    GLFW_MOUSE_BUTTON_MIDDLE,
    [this, &app]() {
      panning = false;
      app.SetCursorLocked(false);
      const auto pos = app.GetMousePosition();
      ImGui::GetIO().AddMousePosEvent(pos.x, pos.y);
    },
    false);
}
auto CameraController::Update(Application &app) -> void {
  auto dirty = false;
  dirty |= UpdatePosition(app);
  dirty |= mouselook && UpdateRotation(app);
  dirty |= orbiting && UpdateOrbit(app);
  dirty |= panning && UpdatePan(app);
  dirty |= UpdateZoom(app);
  if (app.IsBindingPressed(BindingName(Focus)))
    app.AlignView(app.GetSelectedEntity());
  camera.dirty += dirty;
  auto cameraPtr = app.GetEntityComponent<Camera>(entityId);
  if (cameraPtr) {
    if (cameraMissing || cameraPtr->dirty > camera.dirty)
      camera = *cameraPtr;
    else if (camera.dirty > cameraPtr->dirty)
      *cameraPtr = camera;
    // NOTE: if `dirty` wraps around, this will temporarily misbehave
    cameraMissing = false;
  } else
    cameraMissing = true;
  camera.Update();
}
auto CameraController::UpdateOrbit(Application &app) -> bool {
  static constexpr auto EPSILON = 1e-6f;
  static constexpr auto MAX_PITCH = glm::radians(89.f);
  static constexpr auto WORLD_UP = glm::vec3(0.f, 1.f, 0.f);
  static constexpr auto WORLD_RIGHT = glm::vec3(1.f, 0.f, 0.f);
  const auto mousePos = app.GetMousePosition();
  if (mouseEnter) {
    mouseLast = mousePos;
    pitch = glm::asin(std::clamp(camera.forward.y, -1.f, 1.f));
    yaw = std::atan2(-camera.forward.x, -camera.forward.z);
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
  camera.rotation = glm::normalize(glm::angleAxis(yaw, WORLD_UP) * glm::angleAxis(pitch, WORLD_RIGHT));
  const auto newForward = glm::normalize(camera.rotation * glm::vec3(0.f, 0.f, -1.f));
  camera.position = orbitPivot - newForward * orbitDistance;
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
  camera.position += (-camera.right * mouseDiff.x + camera.up * mouseDiff.y) * settings.panSpeed;
  return true;
}
auto CameraController::UpdatePosition(Application &app) -> bool {
  constexpr auto WORLD_UP = glm::vec3(.0f, 1.f, .0f);
  if (!app.IsViewportHovered() && !mouselook)
    return false;
  if (app.IsSequenceInProgress())
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
  // NOTE: smoothstep instead of a linear ramp, for a less abrupt, more vehicle-like acceleration feel.
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
  camera.position += (camera.forward * static_cast<float>(wasd.y) + camera.right * static_cast<float>(wasd.x) + WORLD_UP * static_cast<float>(eq)) * velocity;
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
    pitch = glm::asin(std::clamp(camera.forward.y, -1.f, 1.f));
    yaw = std::atan2(-camera.forward.x, -camera.forward.z);
  }
  mouseEnter = false;
  glm::vec2 mouseDiff{};
  mouseDiff.x = (mousePos.x - mouseLast.x) * settings.mouseSensitivity;
  mouseDiff.y = (mouseLast.y - mousePos.y) * settings.mouseSensitivity; // NOTE: y is inverted because (0,0) is the northwest corner
  mouseLast = mousePos;
  if (glm::length2(mouseDiff) < EPSILON)
    return false;
  yaw -= mouseDiff.x; // NOTE: x diff is inverted because positive rotation is counter-clockwise when looking in the direction of the axis
  pitch = std::clamp(pitch + mouseDiff.y, -MAX_PITCH, MAX_PITCH);
  camera.rotation = glm::normalize(glm::angleAxis(yaw, WORLD_UP) * glm::angleAxis(pitch, WORLD_RIGHT));
  return true;
}
auto CameraController::UpdateZoom(Application &app) -> bool {
  static constexpr auto EPSILON = 1e-6f;
  static constexpr auto MIN_ORTHO_SIZE = .1f;
  if (!app.IsViewportHovered() && !mouselook)
    return false;
  const auto scroll = app.GetScrollOffset().y;
  if (glm::abs(scroll) < EPSILON)
    return false;
  auto zoomSpeed = settings.zoomSpeed;
  if (app.IsBindingHeld(BindingName(Precision)))
    zoomSpeed *= settings.precisionMin;
  else if (app.IsBindingHeld(BindingName(Boost)))
    zoomSpeed *= settings.moveBoostMax;
  if (camera.type == CameraType::Orthographic)
    camera.orthoSize = std::max(MIN_ORTHO_SIZE, camera.orthoSize - scroll * zoomSpeed);
  else
    camera.position += camera.forward * scroll * zoomSpeed;
  return true;
}
