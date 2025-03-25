#include <editor.hpp>
#include <camera_controller.hpp>
#include <component/camera.hpp>
#include <component/transform.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/quaternion_float.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <ImGuizmo.h>
void Editor::DrawGizmos(float width, float height) {
  auto camera = cameraController.GetCamera();
  if (!camera)
    return;
  if (currentSelection < 0)
    return;
  auto transform = GetComponent<Transform>(currentSelection);
  if (!transform)
    return;
  ImGuizmo::SetDrawlist();
  ImGuizmo::SetOrthographic(false);
  auto windowPos = ImGui::GetWindowPos();
  ImGuizmo::SetRect(windowPos.x, windowPos.y, width, height);
  glm::mat4 matrix;
  auto rotation = glm::degrees(glm::eulerAngles(transform->rotation));
  ImGuizmo::RecomposeMatrixFromComponents(glm::value_ptr(transform->position), glm::value_ptr(rotation), glm::value_ptr(transform->scale), glm::value_ptr(matrix));
  ImGuizmo::Manipulate(glm::value_ptr(camera->view), glm::value_ptr(camera->projection), ImGuizmo::OPERATION::UNIVERSAL, ImGuizmo::MODE::WORLD, glm::value_ptr(matrix));
  ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(matrix), glm::value_ptr(transform->position), glm::value_ptr(rotation), glm::value_ptr(transform->scale));
  transform->rotation = glm::quat(glm::radians(rotation));
  transform->dirty = true;
}
