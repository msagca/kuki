#include <asset_loader.hpp>
#include <asset_manager.hpp>
#include <camera_controller.hpp>
#include <component_types.hpp>
#include <entity_manager.hpp>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/ext/vector_float3.hpp>
#include <gui.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <input_manager.hpp>
#include <iostream>
#include <stb_image.h>
#include <system.hpp>
static const auto WINDOW_HEIGHT = 600u;
static const auto WINDOW_WIDTH = 800u;
static const auto CAMERA_POSITION = glm::vec3(.0f, .0f, 3.0f);
static const auto INACTIVITY_TIMEOUT = 3.0f;
static const auto FPS_UPDATE_INTERVAL = .1;
double deltaTime = .0;
static double elapsedTime = .0;
static unsigned int frameCount = 0;
static unsigned int fps = 0;
bool showCreateMenu = false;
bool showHierarchyWindow = true;
bool showFPS = true;
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
  EntityManager entityManager;
  AssetManager assetManager;
  AssetLoader assetLoader(assetManager);
  RenderSystem renderSystem(entityManager, assetManager, assetLoader);
  // populate the scene
  auto cameraID = entityManager.Create("MainCamera");
  auto& camera = entityManager.AddComponent<Camera>(cameraID);
  auto cameraController = CameraController(camera, inputManager);
  cameraController.SetPosition(CAMERA_POSITION);
  cameraControllerPtr = &cameraController;
  renderSystem.SetCamera(&camera);
  auto lightID = entityManager.Create("MainLight");
  entityManager.AddComponent<Light>(lightID);
  lightID = entityManager.Create("PointLight");
  auto& light = entityManager.AddComponent<Light>(lightID);
  light.type = LightType::Point;
  light.vector = glm::vec3(-.2f, 1.0f, -.3f);
  glfwMaximizeWindow(window);
  // register callbacks
  inputManager.RegisterCallback(GLFW_KEY_F, GLFW_PRESS, []() { showFPS = !showFPS; });
  inputManager.RegisterCallback(GLFW_KEY_H, GLFW_PRESS, []() { showHierarchyWindow = !showHierarchyWindow; });
  inputManager.RegisterCallback(GLFW_KEY_R, GLFW_PRESS, [&renderSystem]() { renderSystem.ToggleWireframeMode(); });
  inputManager.RegisterCallback(GLFW_KEY_SPACE, GLFW_PRESS, []() { showCreateMenu = !showCreateMenu; });
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
    cameraController.Update();
    renderSystem.Update();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    if (showFPS)
      ShowFPS(fps);
    if (showHierarchyWindow)
      ShowHierarchyWindow(entityManager, inputManager);
    if (showCreateMenu)
      ShowCreateMenu(entityManager, assetManager, assetLoader);
    if (inputManager.GetInactivityTime() > INACTIVITY_TIMEOUT)
      ShowHints();
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
