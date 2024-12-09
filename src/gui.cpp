#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <primitive.hpp>
#include <mesh.hpp>
#include <gui.hpp>
extern bool showCreateMenu;
extern bool showHierarchyWindow;
extern double deltaTime;
static auto idleTime = .0;
static auto showHints = false;
static const auto HINT_OFFSET = 10.0f;
static const auto INACTIVITY_DURATION = 5.0;
struct Hint {
  std::string text;
  std::function<bool()> condition;
};
static const std::vector<Hint> hints = {
  {"Hold down the right mouse button and use the WASD keys to fly around.", []() { return true; }},
  {"Press spacebar to open/close the create menu.", []() { return !showCreateMenu; }},
  {"Press H to show/hide the hierarchy window.", []() { return !showHierarchyWindow; }}};
void ShowCreateMenu(EntityManager& entityManager) {
  if (!showCreateMenu)
    return;
  static const char* primitives[] = {"Cube"};
  static int selectedPrimitive = -1;
  static auto position = glm::vec3(.0f);
  static auto rotation = glm::vec3(.0f);
  static auto scale = glm::vec3(1.0f);
  static bool showProperties = false;
  ImGui::Begin("Create");
  if (ImGui::ListBox("Primitives", &selectedPrimitive, primitives, IM_ARRAYSIZE(primitives)))
    showProperties = true;
  if (showProperties && selectedPrimitive != -1) {
    ImGui::InputFloat3("Position", glm::value_ptr(position));
    ImGui::InputFloat3("Rotation", glm::value_ptr(rotation));
    ImGui::InputFloat3("Scale", glm::value_ptr(scale));
    if (ImGui::Button("Confirm")) {
      auto id = entityManager.CreateEntity();
      std::vector<float> vertices;
      std::vector<unsigned int> indices;
      if (strcmp(primitives[selectedPrimitive], "Cube") == 0) {
        vertices = Cube::GetVertices();
        indices = Cube::GetIndices();
      }
      auto mesh = CreateMesh(vertices, indices);
      auto [transform, filter, _] = entityManager.AddComponents<Transform, MeshFilter, MeshRenderer>(id);
      filter = mesh;
      transform.position = position;
      transform.rotation = rotation;
      transform.scale = scale;
      selectedPrimitive = -1;
      showProperties = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      selectedPrimitive = -1;
      showProperties = false;
    }
  }
  ImGui::End();
}
static void ShowProperties(EntityManager& entityManager, unsigned int selectedEntity) {
  ImGui::Begin("Properties");
  auto components = entityManager.GetAllComponents(selectedEntity);
  for (const auto& component : components) {
    ImGui::Text(component->GetName().c_str());
    auto properties = component->GetProperties();
    for (const auto& prop : properties)
      if (std::holds_alternative<glm::vec3>(prop.value)) {
        auto value = std::get<glm::vec3>(prop.value);
        ImGui::InputFloat3(prop.name.c_str(), glm::value_ptr(value));
      } else if (std::holds_alternative<GLuint>(prop.value)) {
        auto value = static_cast<int>(std::get<GLuint>(prop.value));
        ImGui::InputInt(prop.name.c_str(), &value);
      } else if (std::holds_alternative<GLsizei>(prop.value)) {
        auto value = static_cast<int>(std::get<GLsizei>(prop.value));
        ImGui::InputInt(prop.name.c_str(), &value);
      }
  }
  ImGui::End();
}
void ShowHierarchyWindow(EntityManager& entityManager) {
  if (!showHierarchyWindow)
    return;
  static int selectedEntity = -1;
  ImGui::Begin("Hierarchy");
  entityManager.ForEach<Transform>([&](unsigned int id) {
    if (ImGui::Selectable(entityManager.GetName(id).c_str(), id == selectedEntity))
      selectedEntity = id;
  });
  auto windowHeight = ImGui::GetWindowHeight();
  auto buttonHeight = ImGui::GetFrameHeight();
  auto padding = ImGui::GetStyle().WindowPadding.y;
  ImGui::SetCursorPosY(windowHeight - buttonHeight - padding);
  if (selectedEntity != -1)
    if (ImGui::Button("Remove")) {
      entityManager.RemoveEntity(selectedEntity);
      selectedEntity = -1;
    }
  ImGui::End();
  if (selectedEntity != -1)
    ShowProperties(entityManager, selectedEntity);
}
void ShowHints(float windowWidth, float windowHeight) {
  if (!showHints)
    return;
  ImVec2 position(HINT_OFFSET, windowHeight - HINT_OFFSET);
  ImVec2 size(windowWidth - 2 * HINT_OFFSET, .0f);
  ImGui::SetNextWindowPos(position, ImGuiCond_Always, ImVec2(.0f, 1.0f));
  ImGui::SetNextWindowSize(size);
  ImGui::SetNextWindowBgAlpha(0.5f);
  ImGui::Begin("Hints", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove);
  for (const auto& hint : hints)
    if (hint.condition())
      ImGui::TextWrapped("%s", hint.text.c_str());
  ImGui::End();
}
void TrackUserActivity(GLFWwindow* window) {
  for (auto key = GLFW_KEY_SPACE; key <= GLFW_KEY_LAST; key++)
    if (glfwGetKey(window, key) == GLFW_PRESS) {
      idleTime = 0.0f;
      showHints = false;
    }
  for (auto button = GLFW_MOUSE_BUTTON_1; button <= GLFW_MOUSE_BUTTON_LAST; button++)
    if (glfwGetMouseButton(window, button) == GLFW_PRESS) {
      idleTime = 0.0f;
      showHints = false;
    }
  idleTime += deltaTime;
  if (idleTime >= INACTIVITY_DURATION)
    showHints = true;
}
