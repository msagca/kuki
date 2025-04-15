#define IMGUI_ENABLE_FREETYPE
#include <application.hpp>
#include <command.hpp>
#include <component/camera.hpp>
#include <component/light.hpp>
#include <editor.hpp>
#include <event_dispatcher.hpp>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_freetype.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h>
#include <imgui_sink.hpp>
#include <memory>
#include <mutex>
#include <primitive.hpp>
#include <render_system.hpp>
#include <spdlog/logger.h>
#include <spdlog/spdlog.h>
#include <string>
// NOTE: this comment is to prevent the following from being placed before imgui.h during includes sorting
#include <imfilebrowser.h>
#include <ImGuizmo.h>
Editor::Editor()
  : Application("Editor"), cameraController(inputManager), imguiSink(std::make_shared<ImGuiSink<std::mutex>>()), logger(std::make_shared<spdlog::logger>("Logger", imguiSink)) {}
void Editor::Start() {
  RegisterInputCallback(GLFW_MOUSE_BUTTON_2, GLFW_PRESS, [&]() { glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); }, "Disable cursor.");
  RegisterInputCallback(GLFW_MOUSE_BUTTON_2, GLFW_RELEASE, [&]() { glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL); }, "Enable cursor.");
  RegisterInputCallback(GLFW_KEY_V, GLFW_PRESS, RenderSystem::ToggleWireframeMode, "Toggle wireframe mode.");
  RegisterEventCallback<EntityCreatedEvent>([this](const EntityCreatedEvent& event) { EntityCreatedCallback(event); });
  RegisterEventCallback<EntityDeletedEvent>([this](const EntityDeletedEvent& event) { EntityDeletedCallback(event); });
  InitImGui();
  LoadDefaultAssets();
  LoadDefaultScene();
  cameraController.SetCamera(&editorCamera);
  CreateSystem<RenderSystem>(static_cast<Application&>(*this));
  RegisterCommand(new SpawnCommand());
  RegisterCommand(new DeleteCommand());
  spdlog::register_logger(logger);
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
  // DisplayLogs();
  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
void Editor::InitLayout() {
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
  std::string entityName = "MainCamera";
  auto entityId = CreateEntity(entityName);
  auto camera = AddComponent<Camera>(entityId);
  camera->Update();
  entityName = "MainLight";
  entityId = CreateEntity(entityName);
  AddComponent<Light>(entityId);
}
void Editor::LoadDefaultAssets() {
  LoadPrimitive(PrimitiveId::Cube);
  LoadPrimitive(PrimitiveId::CubeInverted);
  LoadPrimitive(PrimitiveId::Cylinder);
  LoadPrimitive(PrimitiveId::Frame);
  LoadPrimitive(PrimitiveId::Plane);
  LoadPrimitive(PrimitiveId::Sphere);
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
  auto builder = ImGuiFreeType::GetBuilderForFreeType();
  io.Fonts->Clear();
  io.Fonts->AddFontDefault();
  builder->FontBuilder_Build(io.Fonts);
  fileBrowser.SetTitle("Browse Files");
  fileBrowser.SetTypeFilters({".gltf"});
}
