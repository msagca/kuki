#include <application.hpp>
#include <asset_loader.hpp>
#include <component/camera.hpp>
#include <component/light.hpp>
#include <component/material.hpp>
#include <component/transform.hpp>
#include <editor.hpp>
#include <entity_manager.hpp>
#include <glad/glad.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h>
#include <imfilebrowser.h>
#include <ImGuizmo.h>
#include <input_manager.hpp>
#include <primitive.hpp>
#include <render_system.hpp>
#include <scene.hpp>
#include <stb_image.h>
#include <string>
Editor::Editor()
  : Application("Editor"), cameraController(inputManager), texturePool(CreateTexture, DeleteTexture, 16) {}
void Editor::Start() {
  SetKeyCallback(GLFW_MOUSE_BUTTON_2, GLFW_PRESS, [&]() { glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); }, "Disable cursor.");
  SetKeyCallback(GLFW_MOUSE_BUTTON_2, GLFW_RELEASE, [&]() { glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL); }, "Enable cursor.");
  SetKeyCallback(GLFW_KEY_V, GLFW_PRESS, RenderSystem::ToggleWireframeMode, "Toggle wireframe mode.");
  InitImGui();
  LoadDefaultAssets();
  LoadDefaultScene();
  CreateSystem<RenderSystem>(assetManager);
  Application::Start();
}
void Editor::Update() {
  UpdateView();
  Application::Update();
}
void Editor::Shutdown() {
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
  Application::Shutdown();
}
void Editor::UpdateView() {
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  ImGuizmo::BeginFrame();
  ImGui::DockSpaceOverViewport(ImGui::GetID("DockSpace"));
  InitLayout();
  DisplayAssets();
  DisplayHierarchy();
  DisplayScene();
  DisplayConsole();
  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
void Editor::InitLayout() {
  static bool firstRun = true;
  if (!firstRun)
    return;
  firstRun = false;
  auto viewport = ImGui::GetMainViewport();
  auto dockspaceID = ImGui::GetID("DockSpace");
  auto viewportSize = viewport->Size;
  ImGui::DockBuilderRemoveNode(dockspaceID);
  ImGui::DockBuilderAddNode(dockspaceID, ImGuiDockNodeFlags_DockSpace);
  ImGui::DockBuilderSetNodeSize(dockspaceID, viewportSize);
  auto mainID = dockspaceID;
  ImGui::DockBuilderDockWindow("Scene", mainID);
  auto rightID = ImGui::DockBuilderSplitNode(mainID, ImGuiDir_Right, .3f, nullptr, &mainID);
  ImGui::DockBuilderDockWindow("Hierarchy", rightID);
  auto rightBottomID = ImGui::DockBuilderSplitNode(rightID, ImGuiDir_Down, .5f, nullptr, &rightID);
  ImGui::DockBuilderDockWindow("Properties", rightBottomID);
  auto bottomID = ImGui::DockBuilderSplitNode(mainID, ImGuiDir_Down, .3f, nullptr, &mainID);
  ImGui::DockBuilderDockWindow("Assets", bottomID);
  ImGui::DockBuilderFinish(dockspaceID);
}
void Editor::LoadDefaultScene() {
  auto scene = CreateScene("Main");
  std::string entityName = "MainCamera";
  auto entityID = CreateEntity(entityName);
  AddComponent<Camera>(entityID);
  entityName = "MainLight";
  entityID = CreateEntity(entityName);
  AddComponent<Light>(entityID);
}
void Editor::LoadDefaultAssets() {
  LoadPrimitive(PrimitiveID::Cube);
  LoadPrimitive(PrimitiveID::Sphere);
  LoadPrimitive(PrimitiveID::Cylinder);
  LoadPrimitive(PrimitiveID::Plane);
  LoadPrimitive(PrimitiveID::CubeInverted);
  std::string assetName = "Skybox";
  LoadCubeMap(assetName, "image/skybox/top.jpg", "image/skybox/bottom.jpg", "image/skybox/right.jpg", "image/skybox/left.jpg", "image/skybox/front.jpg", "image/skybox/back.jpg");
}
void Editor::InitImGui() {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  auto& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad | ImGuiConfigFlags_DockingEnable;
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init();
  ImGuizmo::SetImGuiContext(ImGui::GetCurrentContext());
  fileBrowser.SetTitle("Browse Files");
  fileBrowser.SetTypeFilters({".gltf"});
}
unsigned int Editor::CreateTexture() {
  unsigned int id;
  glGenTextures(1, &id);
  return id;
}
void Editor::DeleteTexture(unsigned int id) {
  glDeleteTextures(1, &id);
}
void Editor::UpdateThumbnails() {
  updateThumbnails = true;
}
