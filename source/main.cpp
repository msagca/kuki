#define STB_IMAGE_IMPLEMENTATION
#include "editor.hpp"
#include <asset_loader.hpp>
#include <asset_manager.hpp>
#include <camera_controller.hpp>
#include <entity_manager.hpp>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui_impl_glfw.h>
#include <input_manager.hpp>
#include <iostream>
#include <ostream>
#include <render_system.hpp>
#include <stb_image.h>
static const auto FPS_UPDATE_INTERVAL = .1;
double deltaTime = .0;
static double elapsedTime = .0;
static unsigned int frameCount = 0;
static unsigned int fps = 0;
static CameraController* cameraControllerPtr;
static GLFWwindow* InitGLFW(unsigned int, unsigned int);
static void InitGL(unsigned int, unsigned int);
static void WindowCloseCallback(GLFWwindow*);
static void FramebufferSizeCallback(GLFWwindow*, int, int);
static void SetWindowIcon(GLFWwindow*, const char*);
int main() {
  auto width = 800u;
  auto height = 600u;
  auto window = InitGLFW(width, height);
  if (!window)
    return 1;
  InitGL(width, height);
  AssetManager assetManager;
  AssetLoader assetLoader(assetManager);
  EntityManager entityManager(assetManager);
  CameraController cameraController(entityManager);
  cameraControllerPtr = &cameraController;
  RenderSystem renderSystem(entityManager, assetManager, cameraController);
  Editor editor(assetManager, assetLoader, entityManager, cameraController, renderSystem);
  InputManager::GetInstance().SetWindowCallbacks(window);
  InputManager::GetInstance().RegisterCallback(GLFW_MOUSE_BUTTON_2, GLFW_PRESS, [&window]() { glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); });
  InputManager::GetInstance().RegisterCallback(GLFW_MOUSE_BUTTON_2, GLFW_RELEASE, [&window]() { glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL); });
  glfwMaximizeWindow(window);
  auto timeLast = .0;
  editor.Init(window);
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
    editor.Render();
    glfwSwapBuffers(window);
    glfwPollEvents();
  }
  editor.CleanUp();
  renderSystem.CleanUp();
  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
static GLFWwindow* InitGLFW(unsigned int width, unsigned int height) {
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
  auto window = glfwCreateWindow(width, height, "Learning OpenGL", nullptr, nullptr);
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
static void InitGL(unsigned int width, unsigned int height) {
  glViewport(0, 0, width, height);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  glFrontFace(GL_CCW);
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
