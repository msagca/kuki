#define STB_IMAGE_IMPLEMENTATION
#include <application.hpp>
#include <asset_loader.hpp>
#include <asset_manager.hpp>
#include <component/camera.hpp>
#include <component/light.hpp>
#include <component/material.hpp>
#include <component/transform.hpp>
#include <editor.hpp>
#include <entity_manager.hpp>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imfilebrowser.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <ImGuizmo.h>
#include <input_manager.hpp>
#include <iostream>
#include <primitive.hpp>
#include <render_system.hpp>
#include <scene.hpp>
#include <stb_image.h>
static const auto WINDOW_FLAGS = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
Editor::Editor()
  : inputManager(), cameraController(inputManager), texturePool(CreateTexture, DeleteTexture, 16) {}
void Editor::Start() {
  inputManager.RegisterCallback(GLFW_MOUSE_BUTTON_2, GLFW_PRESS, [&]() { glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); }, "Disable cursor.");
  inputManager.RegisterCallback(GLFW_MOUSE_BUTTON_2, GLFW_RELEASE, [&]() { glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL); }, "Enable cursor.");
  auto width = 800;
  auto height = 600;
  InitOpenGL(width, height);
  glfwSetKeyCallback(window, KeyCallback);
  glfwSetMouseButtonCallback(window, MouseButtonCallback);
  glfwSetCursorPosCallback(window, CursorPosCallback);
  InitImGui();
  ImGuizmo::SetImGuiContext(ImGui::GetCurrentContext());
  fileDialog.SetTitle("Browse Files");
  fileDialog.SetTypeFilters({".gltf", ".fbx"});
  LoadDefaultAssets();
  LoadDefaultScene();
  CreateSystem<RenderSystem>(assetManager);
  Application::Start();
}
bool Editor::Status() {
  return !glfwWindowShouldClose(window);
}
void Editor::Update() {
  Application::Update();
  UpdateView();
}
void Editor::Shutdown() {
  Application::Shutdown();
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
  glfwDestroyWindow(window);
  glfwTerminate();
}
void Editor::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
  auto instance = static_cast<Editor*>(glfwGetWindowUserPointer(window));
  if (instance)
    instance->inputManager.KeyCallback(window, key, scancode, action, mods);
}
void Editor::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
  auto instance = static_cast<Editor*>(glfwGetWindowUserPointer(window));
  if (instance)
    instance->inputManager.MouseButtonCallback(window, button, action, mods);
}
void Editor::CursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
  auto instance = static_cast<Editor*>(glfwGetWindowUserPointer(window));
  if (instance)
    instance->inputManager.CursorPosCallback(window, xpos, ypos);
}
void Editor::UpdateView() {
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  ImGui::DockSpaceOverViewport(ImGui::GetID("DockSpace"));
  DisplayResources();
  DisplayHierarchy();
  auto scene = GetActiveScene();
  cameraController.SetCamera(scene->GetEntityManager().GetFirstComponent<Camera>());
  cameraController.Update(deltaTime);
  DisplayScene();
  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
  glfwSwapBuffers(window);
  glfwPollEvents();
}
void Editor::LoadDefaultScene() {
  auto scene = CreateScene();
  auto& entityManager = scene->GetEntityManager();
  auto cameraID = entityManager.Create("MainCamera");
  entityManager.AddComponent<Camera>(cameraID);
  auto lightID = entityManager.Create("MainLight");
  entityManager.AddComponent<Light>(lightID);
}
void Editor::LoadDefaultAssets() {
  assetLoader.LoadShader("DefaultLit", "shader/default_lit.vert", "shader/default_lit.frag");
  assetLoader.LoadShader("DefaultUnlit", "shader/default_unlit.vert", "shader/default_unlit.frag");
  auto id = assetLoader.LoadMaterial("DefaultMaterial", "image/T_umonab2dy_1K_B.jpg", "image/T_umonab2dy_1K_N.jpg", "image/T_umonab2dy_1K_ORM.jpg");
  Material material;
  auto materialPtr = assetManager.GetComponent<Material>(id);
  if (!materialPtr)
    std::cerr << "Unable to load the default material." << std::endl;
  else
    material = *materialPtr;
  id = assetLoader.LoadMesh("Cube", Primitive::Cube());
  assetManager.AddComponent<Transform>(id);
  *assetManager.AddComponent<Material>(id) = material;
  id = assetLoader.LoadMesh("Sphere", Primitive::Sphere());
  assetManager.AddComponent<Transform>(id);
  *assetManager.AddComponent<Material>(id) = material;
  id = assetLoader.LoadMesh("Cylinder", Primitive::Cylinder());
  assetManager.AddComponent<Transform>(id);
  *assetManager.AddComponent<Material>(id) = material;
}
void Editor::InitOpenGL(int width, int height) {
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
  window = glfwCreateWindow(width, height, "Editor", nullptr, nullptr);
  if (!window) {
    std::cerr << "Editor: Failed to create GLFW window." << std::endl;
    glfwTerminate();
    return;
  }
  glfwMakeContextCurrent(window);
  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    std::cerr << "Editor: Failed to initialize GLAD." << std::endl;
    return;
  }
  auto version = glGetString(GL_VERSION);
  std::cout << "OpenGL version: " << version << std::endl;
  int major, minor;
  glGetIntegerv(GL_MAJOR_VERSION, &major);
  glGetIntegerv(GL_MINOR_VERSION, &minor);
  if (major < 4 || (major == 4 && minor < 6)) {
    std::cerr << "OpenGL 4.6 or higher is required." << std::endl;
    return;
  }
  auto error = glGetError();
  if (error != GL_NO_ERROR) {
    std::cerr << "OpenGL error after initialization: " << error << std::endl;
    return;
  }
  glfwSwapInterval(0);
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
  glfwSetWindowUserPointer(window, this);
  glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);
  glfwSetWindowCloseCallback(window, WindowCloseCallback);
  SetWindowIcon("image/logo.png");
  glViewport(0, 0, width, height);
  glfwMaximizeWindow(window);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  glFrontFace(GL_CCW);
}
void Editor::InitImGui() {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  auto& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad | ImGuiConfigFlags_DockingEnable;
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init();
}
void Editor::WindowCloseCallback(GLFWwindow* window) {
  glfwSetWindowShouldClose(window, GLFW_TRUE);
}
void Editor::FramebufferSizeCallback(GLFWwindow* window, int width, int height) {
  glViewport(0, 0, width, height);
  auto ratio = static_cast<float>(width) / height;
  auto editor = static_cast<Editor*>(glfwGetWindowUserPointer(window));
  if (editor)
    editor->cameraController.SetAspect(ratio);
}
void Editor::SetWindowIcon(const char* iconPath) {
  int width, height, channels;
  unsigned char* data = stbi_load(iconPath, &width, &height, &channels, 4);
  if (data) {
    GLFWimage images[1]{};
    images[0].width = width;
    images[0].height = height;
    images[0].pixels = data;
    glfwSetWindowIcon(window, 1, images);
    stbi_image_free(data);
  } else
    std::cerr << "Failed to load the icon at " << iconPath << "." << std::endl;
}
unsigned int Editor::CreateTexture() {
  unsigned int id;
  glGenTextures(1, &id);
  return id;
}
void Editor::DeleteTexture(unsigned int id) {
  glDeleteTextures(1, &id);
}
void Editor::AssetAdditionHandler() {
  updateThumbnails = true;
}
