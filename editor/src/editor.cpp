#include <app_config.hpp>
#include <application.hpp>
#include <camera.hpp>
#include <camera_controller.hpp>
#include <editor.hpp>
#include <glfw_constants.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h>
#include <imgui_sink.hpp>
#include <light.hpp>
#include <memory>
#include <mesh.hpp>
#include <mesh_filter.hpp>
#include <mesh_renderer.hpp>
#include <mutex>
#include <primitive.hpp>
#include <rendering_system.hpp>
#include <skybox.hpp>
#include <spdlog/logger.h>
#include <string>
#include <transform.hpp>
//
#include <GLFW/glfw3.h>
#include <ImGuizmo.h>
#include <imfilebrowser.h>
using namespace kuki;
Editor::Editor()
  : Application(AppConfig{"Kuki Editor", "image/logo.png"}), imguiSink(std::make_shared<ImGuiSink<std::mutex>>()), logger(std::make_shared<spdlog::logger>("Logger", imguiSink)) {}
Editor::~Editor() {
  Application::Shutdown();
}
void Editor::Init() {
  Application::Init();
  RegisterInputAction("gc", [this]() { ToggleGizmo(GizmoType::FrustumCulling); });
  RegisterInputAction("gf", [this]() { ToggleGizmo(GizmoType::ViewFrustum); });
  RegisterInputAction("gm", [this]() { ToggleGizmo(GizmoType::Manipulator); });
  RegisterInputAction(GLFWConst::KEY_V, RenderingSystem::ToggleWireframeMode);
  RegisterInputAction(GLFWConst::MOUSE_BUTTON_RIGHT, [this]() {
    cameraController->SetMouselook(true);
    glfwSetInputMode(window, GLFWConst::CURSOR, GLFWConst::CURSOR_DISABLED);
    auto& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NoMouse; });
  RegisterInputAction(GLFWConst::MOUSE_BUTTON_RIGHT, [this]() {
    cameraController->SetMouselook(false);
    glfwSetInputMode(window, GLFWConst::CURSOR, GLFWConst::CURSOR_NORMAL);
    auto& io = ImGui::GetIO();
    io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse; }, false);
  InitImGui();
  CreateSystem<RenderingSystem>(*this);
  RegisterCommand(new SpawnCommand(*this));
  RegisterCommand(new DeleteCommand(*this));
  // spdlog::register_logger(logger);
}
void Editor::Start() {
  Application::Start();
  LoadDefaultAssets();
  LoadDefaultScene();
}
void Editor::Update() {
  Application::Update();
  cameraController->Update(deltaTime);
}
void Editor::LateUpdate() {
  Application::LateUpdate();
  UpdateView();
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
  DisplayHierarchy();
  DisplayAssets();
  DisplayScene();
  // DisplayLogs();
  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
void Editor::InitLayout() {
  // TODO: if an imgui.ini file exists, restore the layout from it
  static bool firstRun = true;
  if (!firstRun)
    return;
  firstRun = false;
  auto viewport = ImGui::GetMainViewport();
  auto dockspaceId = ImGui::GetID("DockSpace");
  auto viewportSize = viewport->Size;
  ImGui::DockBuilderRemoveNode(dockspaceId);
  ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
  ImGui::DockBuilderSetNodeSize(dockspaceId, viewportSize);
  auto mainId = dockspaceId;
  ImGui::DockBuilderDockWindow("Scene", mainId);
  auto rightId = ImGui::DockBuilderSplitNode(mainId, ImGuiDir_Right, .3f, nullptr, &mainId);
  ImGui::DockBuilderDockWindow("Hierarchy", rightId);
  auto rightBottomId = ImGui::DockBuilderSplitNode(rightId, ImGuiDir_Down, .5f, nullptr, &rightId);
  ImGui::DockBuilderDockWindow("Properties", rightBottomId);
  auto bottomId = ImGui::DockBuilderSplitNode(mainId, ImGuiDir_Down, .3f, nullptr, &mainId);
  ImGui::DockBuilderDockWindow("Assets", bottomId);
  // ImGui::DockBuilderDockWindow("Logs", bottomId);
  ImGui::DockBuilderFinish(dockspaceId);
}
void Editor::LoadDefaultScene() {
  auto scene = CreateScene("Main");
  std::string entityName = "Camera";
  auto entityId = CreateEntity(entityName);
  auto camera = AddEntityComponent<Camera>(entityId);
  cameraController = std::make_unique<CameraController>(*this, entityId);
  entityName = "Cube";
  entityId = CreateEntity(entityName);
  auto filter = AddEntityComponent<MeshFilter>(entityId);
  filter->mesh = *GetAssetComponent<Mesh>(GetAssetId("Cube"));
  AddEntityComponent<MeshRenderer>(entityId);
  AddEntityComponent<Transform>(entityId);
  camera->Frame(filter->mesh.bounds, 2.f);
  entityName = "Skybox";
  entityId = CreateEntity(entityName);
  filter = AddEntityComponent<MeshFilter>(entityId);
  filter->mesh = *GetAssetComponent<Mesh>(GetAssetId("CubeInverted"));
  AddEntityComponent<Skybox>(entityId);
  entityName = "Light";
  entityId = CreateEntity(entityName);
  AddEntityComponent<Light>(entityId);
}
void Editor::LoadDefaultAssets() {
  LoadPrimitive(PrimitiveType::Cube);
  LoadPrimitive(PrimitiveType::CubeInverted);
  LoadPrimitive(PrimitiveType::Cylinder);
  LoadPrimitive(PrimitiveType::Frame);
  LoadPrimitive(PrimitiveType::Plane);
  LoadPrimitive(PrimitiveType::Sphere);
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
}
