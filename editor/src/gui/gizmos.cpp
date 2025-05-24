#include <system/rendering.hpp>
#include <editor.hpp>
#include <camera_controller.hpp>
#include <component/camera.hpp>
#include <component/light.hpp>
#include <component/transform.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/quaternion_float.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <ImGuizmo.h>
using namespace kuki;
void Editor::DrawGizmos(float width, float height, unsigned int mask) {
  if (selectedEntity < 0)
    return;
  // TODO: for a multi-select, position the gizmo at the center of the selection, and apply manipulations to all entities
  auto manipulatorEnabled = (mask & static_cast<unsigned int>(GizmoMask::Manipulator)) != 0;
  if (!manipulatorEnabled)
    return;
  auto camera = cameraController->GetCamera();
  Transform transform;
  Transform* transformComp = GetEntityComponent<Transform>(selectedEntity);
  Camera* cameraComp = nullptr;
  Light* lightComp = nullptr;
  if (!transformComp) {
    cameraComp = GetEntityComponent<Camera>(selectedEntity);
    if (cameraComp)
      transform = cameraComp->GetTransform();
    else {
      lightComp = GetEntityComponent<Light>(selectedEntity);
      if (lightComp)
        transform = lightComp->GetTransform();
      else
        return;
    }
  } else
    transform = *transformComp;
  ImGuizmo::SetDrawlist();
  ImGuizmo::SetOrthographic(false);
  auto windowPos = ImGui::GetWindowPos();
  ImGuizmo::SetRect(windowPos.x, windowPos.y, width, height);
  glm::mat4 matrix;
  auto rotation = glm::degrees(glm::eulerAngles(transform.rotation));
  ImGuizmo::RecomposeMatrixFromComponents(glm::value_ptr(transform.position), glm::value_ptr(rotation), glm::value_ptr(transform.scale), glm::value_ptr(matrix));
  ImGuizmo::Manipulate(glm::value_ptr(camera->view), glm::value_ptr(camera->projection), ImGuizmo::OPERATION::UNIVERSAL, ImGuizmo::MODE::WORLD, glm::value_ptr(matrix));
  ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(matrix), glm::value_ptr(transform.position), glm::value_ptr(rotation), glm::value_ptr(transform.scale));
  transform.rotation = glm::quat(glm::radians(rotation));
  transform.dirty = true;
  if (cameraComp)
    cameraComp->SetTransform(transform);
  else if (lightComp)
    lightComp->SetTransform(transform);
  else
    *transformComp = transform;
}
void Editor::ToggleGizmos() {
  gizmoMask ^= static_cast<unsigned int>(GizmoMask::Manipulator);
}
