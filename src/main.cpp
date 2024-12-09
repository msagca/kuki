#include <component.hpp>
#include <utility.hpp>
#include <imgui.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui_impl_opengl3.h>
#include <mesh.hpp>
#include <imgui_impl_glfw.h>
#include <entity.hpp>
#include <system.hpp>
#include <iostream>
#include <gui.hpp>
static const auto CAMERA_POSITION = glm::vec3(.0f, .0f, 3.0f);
static const auto CLEAR_COLOR = glm::vec4(0.1f, 0.1f, 0.1f, 1.0f);
Camera* cameraPtr = nullptr;
RenderSystem* renderSystemPtr = nullptr;
double deltaTime = .0;
static GLFWwindow* InitializeGLFW();
static void InitializeImGui(GLFWwindow* window);
int main() {
  auto window = InitializeGLFW();
  InitializeImGui(window);
  // initialize ECS
  EntityManager entityManager;
  RenderSystem renderSystem(entityManager);
  renderSystemPtr = &renderSystem;
  Camera camera(CAMERA_POSITION);
  cameraPtr = &camera;
  renderSystem.SetActiveCamera(&camera);
  auto ratio = static_cast<float>(WINDOW_WIDTH) / WINDOW_HEIGHT;
  renderSystem.SetAspectRatio(ratio);
  auto timeLast = .0;
  while (!glfwWindowShouldClose(window)) {
    auto timeNow = glfwGetTime();
    deltaTime = timeNow - timeLast;
    timeLast = timeNow;
    TrackUserActivity(window);
    ProcessInput(window);
    glClearColor(CLEAR_COLOR.x, CLEAR_COLOR.y, CLEAR_COLOR.z, CLEAR_COLOR.w);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderSystem.Update();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ShowHierarchyWindow(entityManager);
    ShowCreateMenu(entityManager);
    int windowWidth, windowHeight;
    glfwGetFramebufferSize(window, &windowWidth, &windowHeight);
    ShowHints(windowWidth, windowHeight);
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(window);
    glfwPollEvents();
  }
  entityManager.CleanUp();
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
  if (window == nullptr)
    std::cerr << "Error: Failed to create GLFW window." << std::endl;
  glfwMakeContextCurrent(window);
  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    std::cerr << "Error: Failed to initialize GLAD." << std::endl;
  glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
  glEnable(GL_DEPTH_TEST);
  glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
  glfwSetCursorPosCallback(window, MousePosCallback);
  glfwSetMouseButtonCallback(window, MouseButtonCallback);
  glfwSetWindowCloseCallback(window, WindowCloseCallback);
  SetWindowIcon(window, "logo.png");
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
