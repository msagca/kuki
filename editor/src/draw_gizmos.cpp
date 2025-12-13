#include <camera.hpp>
#include <camera_controller.hpp>
#include <component.hpp>
#include <editor.hpp>
#include <glm/ext/quaternion_float.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <light.hpp>
#include <rendering_system.hpp>
#include <transform.hpp>
//
#include <ImGuizmo.h>
using namespace kuki;
void Editor::DrawManipulator(float width, float height) {
  if (!context.selectedEntity.IsValid())
    return;
  auto windowPos = ImGui::GetWindowPos();
  ImGuizmo::SetRect(windowPos.x, windowPos.y, width, height);
  ImGuizmo::SetOrthographic(cameraController->camera.type == CameraType::Orthographic);
  // TODO: for a multi-select, position the gizmo at the center of the selection, and apply manipulations to all entities
  Transform transform;
  auto transformComp = GetEntityComponent<Transform>(context.selectedEntity);
  Camera* cameraComp = nullptr;
  Light* lightComp = nullptr;
  if (!transformComp) {
    cameraComp = GetEntityComponent<Camera>(context.selectedEntity);
    if (cameraComp)
      transform = cameraComp->GetTransform();
    else {
      lightComp = GetEntityComponent<Light>(context.selectedEntity);
      if (lightComp)
        transform = lightComp->GetTransform();
      else
        return;
    }
  } else
    transform = *transformComp;
  ImGuizmo::SetDrawlist();
  if (!ImGuizmo::Manipulate(glm::value_ptr(cameraController->camera.transform.view), glm::value_ptr(cameraController->camera.transform.projection), ImGuizmo::OPERATION::UNIVERSAL, ImGuizmo::MODE::WORLD, glm::value_ptr(transform.local)))
    return;
  glm::vec3 rotation;
  ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(transform.local), glm::value_ptr(transform.position), glm::value_ptr(rotation), glm::value_ptr(transform.scale));
  transform.rotation = glm::quat(glm::radians(rotation));
  if (cameraComp)
    cameraComp->SetTransform(transform);
  else if (lightComp)
    lightComp->SetTransform(transform);
  else
    *transformComp = transform;
}
void Editor::ToggleGizmo(GizmoType type) {
  if (type == GizmoType::Manipulator)
    context.gizmoMask ^= static_cast<unsigned int>(GizmoMask::Manipulator);
  else if (type == GizmoType::ViewFrustum)
    context.gizmoMask ^= static_cast<unsigned int>(GizmoMask::ViewFrustum);
  else if (type == GizmoType::FrustumCulling)
    context.gizmoMask ^= static_cast<unsigned int>(GizmoMask::FrustumCulling);
}
