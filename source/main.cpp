#define STB_IMAGE_IMPLEMENTATION
#include <asset_loader.hpp>
#include <asset_manager.hpp>
#include <camera_controller.hpp>
#include <component/camera.hpp>
#include <component/component.hpp>
#include <component/light.hpp>
#include <entity_manager.hpp>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <gui.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <ImGuizmo.h>
#include <input_manager.hpp>
#include <iostream>
#include <ostream>
#include <render_system.hpp>
#include <stb_image.h>
static const auto WINDOW_HEIGHT = 600u;
static const auto WINDOW_WIDTH = 800u;
static const auto LIGHT_POSITION = glm::vec3(-2.0f, 1.0f, -3.0f);
static const auto INACTIVITY_TIMEOUT = 3.0f;
static const auto FPS_UPDATE_INTERVAL = .1;
static const auto IDENTITY_MATRIX = glm::mat4(1.0f);
double deltaTime = .0;
static double elapsedTime = .0;
static unsigned int frameCount = 0;
static unsigned int fps = 0;
static bool showFPS = false;
static bool showBindings = true;
static bool showHierarchy = true;
static bool showCreateMenu = false;
static bool showGizmo = true;
static CameraController* cameraControllerPtr;
static GLFWwindow* InitializeGLFW();
static void InitializeImGui(GLFWwindow*);
static void WindowCloseCallback(GLFWwindow*);
static void FramebufferSizeCallback(GLFWwindow*, int, int);
static void SetWindowIcon(GLFWwindow*, const char*);
int main() {
  auto window = InitializeGLFW();
  if (!window)
    return 1;
  auto& inputManager = InputManager::GetInstance();
  inputManager.Initialize(window);
  InitializeImGui(window);
  // OpenGL configuration
  glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  glFrontFace(GL_CCW);
  // initialize ECS
  AssetManager assetManager;
  AssetLoader assetLoader(assetManager);
  EntityManager entityManager(assetManager);
  CameraController cameraController(entityManager, inputManager);
  cameraControllerPtr = &cameraController;
  RenderSystem renderSystem(entityManager, assetManager, cameraController);
  // populate the scene
  auto cameraID = entityManager.Create("MainCamera");
  auto camera = entityManager.AddComponent<Camera>(cameraID);
  auto lightID = entityManager.Create("MainLight");
  entityManager.AddComponent<Light>(lightID);
  lightID = entityManager.Create("PointLight");
  auto light = entityManager.AddComponent<Light>(lightID);
  light->type = LightType::Point;
  light->vector = LIGHT_POSITION;
  // assetLoader.LoadModel("Backpack", "model/scene.gltf");
  glfwMaximizeWindow(window);
  // register callbacks
  inputManager.RegisterCallback(GLFW_KEY_G, GLFW_PRESS, []() { showBindings = !showBindings; }, "Show/hide key bindings");
  inputManager.RegisterCallback(GLFW_KEY_H, GLFW_PRESS, []() { showHierarchy = !showHierarchy; }, "Show/hide hierarchy window");
  inputManager.RegisterCallback(GLFW_KEY_E, GLFW_PRESS, []() { showCreateMenu = !showCreateMenu; }, "Show/hide create menu");
  inputManager.RegisterCallback(GLFW_KEY_Q, GLFW_PRESS, []() { showGizmo = !showGizmo; }, "Toggle transform gizmo");
  inputManager.RegisterCallback(GLFW_KEY_R, GLFW_PRESS, [&renderSystem]() { renderSystem.ToggleWireframeMode(); }, "Toggle wireframe mode");
  inputManager.RegisterCallback(GLFW_KEY_F, GLFW_PRESS, []() { showFPS = !showFPS; }, "Toggle FPS");
  inputManager.RegisterCallback(GLFW_MOUSE_BUTTON_2, GLFW_PRESS, [&window]() { glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); });
  inputManager.RegisterCallback(GLFW_MOUSE_BUTTON_2, GLFW_RELEASE, [&window]() { glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL); });
  auto timeLast = .0;
  while (!glfwWindowShouldClose(window)) {
    auto timeNow = glfwGetTime();
    deltaTime = timeNow - timeLast;
    timeLast = timeNow;
    elapsedTime += deltaTime;
    frameCount++;
    if (elapsedTime > FPS_UPDATE_INTERVAL) {
      fps = static_cast<unsigned int>(frameCount / elapsedTime);
      elapsedTime = .0;
      frameCount = 0;
    }
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();
    ImGuizmo::DrawGrid(glm::value_ptr(cameraController.GetView()), glm::value_ptr(cameraController.GetProjection()), glm::value_ptr(IDENTITY_MATRIX), cameraController.GetFar());
    if (showFPS)
      DisplayFPS(fps);
    if (showBindings)
      DisplayKeyBindings(inputManager);
    if (showHierarchy)
      DisplayHierarchy(entityManager, inputManager, cameraController, showGizmo);
    if (showCreateMenu)
      DisplayCreateMenu(entityManager, showCreateMenu);
    renderSystem.Update();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(window);
    glfwPollEvents();
  }
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
static GLFWwindow* InitializeGLFW() {
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
  auto window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Learning OpenGL", nullptr, nullptr);
  if (!window)
    std::cerr << "Failed to create GLFW window." << std::endl;
  glfwMakeContextCurrent(window);
  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    std::cerr << "Failed to initialize GLAD." << std::endl;
  glfwSwapInterval(0);
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
  glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);
  glfwSetWindowCloseCallback(window, WindowCloseCallback);
  SetWindowIcon(window, "image/logo.png");
  return window;
}
static void InitializeImGui(GLFWwindow* window) {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  auto& imguiIO = ImGui::GetIO();
  imguiIO.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  imguiIO.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init();
}
static void WindowCloseCallback(GLFWwindow* window) {
  glfwSetWindowShouldClose(window, GLFW_TRUE);
}
static void FramebufferSizeCallback(GLFWwindow* window, int width, int height) {
  glViewport(0, 0, width, height);
  auto ratio = static_cast<float>(width) / height;
  if (cameraControllerPtr)
    cameraControllerPtr->SetAspect(ratio);
}
static void SetWindowIcon(GLFWwindow* window, const char* iconPath) {
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
