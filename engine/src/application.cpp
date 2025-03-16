#define GLM_ENABLE_EXPERIMENTAL
#define STB_IMAGE_IMPLEMENTATION
#include <application.hpp>
#include <chrono>
#include <cmath>
#include <command.hpp>
#include <component/camera.hpp>
#include <component/component.hpp>
#include <component/material.hpp>
#include <component/mesh.hpp>
#include <component/mesh_filter.hpp>
#include <component/mesh_renderer.hpp>
#include <component/transform.hpp>
#include <entity_manager.hpp>
#include <filesystem>
#include <functional>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/ext/vector_float3.hpp>
#include <iostream>
#include <primitive.hpp>
#include <random>
#include <scene.hpp>
#include <stb_image.h>
#include <string>
#include <system.hpp>
#include <vector>
Application::Application(const std::string& name)
  : name(name), assetManager(), assetLoader(assetManager), inputManager(), sceneManager(), commandManager() {}
const std::string& Application::GetName() const {
  return name;
}
void Application::Configure(const AppConfig& config) {
  this->config = config;
}
const AppConfig& Application::GetConfig() const {
  return config;
}
unsigned int Application::CreateScene(const std::string& name) {
  return sceneManager.Create(name);
}
void Application::DeleteScene(unsigned int id) {
  sceneManager.Delete(id);
}
Scene* Application::GetActiveScene() {
  return sceneManager.Get(activeSceneID);
}
void Application::SetActiveScene(unsigned int id) {
  if (sceneManager.Has(id))
    activeSceneID = id;
}
Camera* Application::GetActiveCamera() {
  auto scene = GetActiveScene();
  if (!scene)
    return nullptr;
  return scene->GetCamera();
}
void Application::Init() {
  static const auto WINDOW_WIDTH = 800.0f;
  static const auto WINDOW_HEIGHT = 600.0f;
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
  glfwWindowHint(GLFW_SAMPLES, 4);
  window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, name.c_str(), nullptr, nullptr);
  if (!window) {
    std::cerr << "Failed to create GLFW window." << std::endl;
    glfwTerminate();
    return;
  }
  glfwMakeContextCurrent(window);
  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    std::cerr << "Failed to initialize GLAD." << std::endl;
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
  SetWindowIcon("image/logo.png");
  glfwSwapInterval(0);
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
  glfwSetWindowUserPointer(window, this);
  glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);
  glfwSetWindowCloseCallback(window, WindowCloseCallback);
  glfwSetKeyCallback(window, KeyCallback);
  glfwSetMouseButtonCallback(window, MouseButtonCallback);
  glfwSetCursorPosCallback(window, CursorPosCallback);
  glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  glFrontFace(GL_CCW);
  glEnable(GL_MULTISAMPLE);
  glfwMaximizeWindow(window);
}
void Application::Start() {
  for (auto system : systems)
    system->Start();
};
bool Application::Status() {
  return !glfwWindowShouldClose(window);
};
void Application::Update() {
  const auto timeNow = std::chrono::high_resolution_clock::now();
  static auto timeLast = timeNow;
  deltaTime = std::chrono::duration<float>(timeNow - timeLast).count();
  timeLast = timeNow;
  auto activeScene = GetActiveScene();
  if (!activeScene)
    return;
  for (auto system : systems)
    system->Update(deltaTime, activeScene);
  glfwSwapBuffers(window);
  glfwPollEvents();
};
void Application::Shutdown() {
  for (const auto system : systems)
    system->Shutdown();
  systems.clear();
  glfwDestroyWindow(window);
  glfwTerminate();
};
void Application::Run() {
  Init();
  Start();
  while (Status())
    Update();
  Shutdown();
}
void Application::RegisterCommand(ICommand* command) {
  commandManager.Register(command);
}
void Application::UnregisterCommand(const std::string& name) {
  commandManager.Unregister(name);
}
int Application::DispatchCommand(const std::string& input, std::string& message) {
  return commandManager.Dispatch(this, input, message);
}
void Application::WindowCloseCallback(GLFWwindow* window) {
  glfwSetWindowShouldClose(window, GLFW_TRUE);
}
void Application::FramebufferSizeCallback(GLFWwindow* window, int width, int height) {
  glViewport(0, 0, width, height);
}
void Application::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
  auto instance = static_cast<Application*>(glfwGetWindowUserPointer(window));
  if (instance)
    instance->inputManager.KeyCallback(window, key, scancode, action, mods);
}
void Application::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
  auto instance = static_cast<Application*>(glfwGetWindowUserPointer(window));
  if (instance)
    instance->inputManager.MouseButtonCallback(window, button, action, mods);
}
void Application::CursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
  auto instance = static_cast<Application*>(glfwGetWindowUserPointer(window));
  if (instance)
    instance->inputManager.CursorPosCallback(window, xpos, ypos);
}
void Application::SetWindowIcon(const std::filesystem::path& path) {
  int width, height, channels;
  auto data = stbi_load(path.string().c_str(), &width, &height, &channels, 4);
  if (data) {
    GLFWimage images[1]{};
    images[0].width = width;
    images[0].height = height;
    images[0].pixels = data;
    glfwSetWindowIcon(window, 1, images);
    stbi_image_free(data);
  } else
    std::cerr << "Failed to load the icon: " << path << std::endl;
}
int Application::CreateEntity(std::string& name) {
  auto scene = GetActiveScene();
  if (!scene)
    return -1;
  return scene->GetEntityManager().Create(name);
}
void Application::DeleteEntity(unsigned int id) {
  auto scene = GetActiveScene();
  if (!scene)
    return;
  scene->GetEntityManager().Delete(id);
}
void Application::DeleteEntity(const std::string& name) {
  auto scene = GetActiveScene();
  if (!scene)
    return;
  scene->GetEntityManager().Delete(name);
}
void Application::DeleteAllEntities() {
  auto scene = GetActiveScene();
  if (!scene)
    return;
  scene->GetEntityManager().DeleteAll();
}
void Application::DeleteAllEntities(const std::string& prefix) {
  auto scene = GetActiveScene();
  if (!scene)
    return;
  scene->GetEntityManager().DeleteAll(prefix);
}
std::string Application::GetEntityName(unsigned int id) {
  auto scene = GetActiveScene();
  if (!scene)
    return "";
  return scene->GetEntityManager().GetName(id);
}
std::string Application::GetAssetName(unsigned int id) {
  return assetManager.GetName(id);
}
int Application::GetEntityID(const std::string& name) {
  auto scene = GetActiveScene();
  if (!scene)
    return -1;
  return scene->GetEntityManager().GetID(name);
}
int Application::GetAssetID(const std::string& name) {
  return assetManager.GetID(name);
}
void Application::RenameEntity(unsigned int id, std::string& name) {
  auto scene = GetActiveScene();
  if (!scene)
    return;
  scene->GetEntityManager().Rename(id, name);
}
IComponent* Application::AddComponent(unsigned int id, const std::string& name) {
  auto scene = GetActiveScene();
  if (!scene)
    return nullptr;
  return scene->GetEntityManager().AddComponent(id, name);
}
void Application::RemoveComponent(unsigned int id, const std::string& name) {
  auto scene = GetActiveScene();
  if (!scene)
    return;
  scene->GetEntityManager().RemoveComponent(id, name);
}
IComponent* Application::GetComponent(unsigned int id, const std::string& name) {
  auto scene = GetActiveScene();
  if (!scene)
    return nullptr;
  return scene->GetEntityManager().GetComponent(id, name);
}
std::vector<IComponent*> Application::GetAllComponents(unsigned int id) {
  auto scene = GetActiveScene();
  if (!scene)
    return std::vector<IComponent*>{};
  return scene->GetEntityManager().GetAllComponents(id);
}
std::vector<std::string> Application::GetMissingComponents(unsigned int id) {
  auto scene = GetActiveScene();
  if (!scene)
    return std::vector<std::string>{};
  return scene->GetEntityManager().GetMissingComponents(id);
}
bool Application::AddChildEntity(unsigned int parent, unsigned int child) {
  auto scene = GetActiveScene();
  if (!scene)
    return false;
  return scene->GetEntityManager().AddChild(parent, child);
}
bool Application::EntityHasParent(unsigned int id) {
  auto scene = GetActiveScene();
  if (!scene)
    return false;
  return scene->GetEntityManager().HasParent(id);
}
bool Application::EntityHasChildren(unsigned int id) {
  auto scene = GetActiveScene();
  if (!scene)
    return false;
  return scene->GetEntityManager().HasChildren(id);
}
glm::vec3 Application::GetRandomPosition(float r) {
  static std::random_device rd;
  static std::mt19937 gen(rd());
  std::uniform_real_distribution<float> dist(.0f, 1.0f);
  auto theta = dist(gen) * 2.0f * glm::pi<float>();
  auto phi = acos(2.0f * dist(gen) - 1.0f);
  auto u = dist(gen);
  auto x = sin(phi) * cos(theta);
  auto y = sin(phi) * sin(theta);
  auto z = cos(phi);
  glm::vec3 direction(x, y, z);
  auto radius = r * std::cbrt(u);
  return direction * radius;
}
int Application::Spawn(std::string& name, int parentID, bool randomPos, float spawnRadius) {
  auto assetID = assetManager.GetID(name);
  if (assetID < 0)
    return -1;
  auto entityID = CreateEntity(name);
  auto components = assetManager.GetAllComponents(assetID);
  for (auto c : components)
    if (auto t = dynamic_cast<Transform*>(c)) {
      auto transform = AddComponent<Transform>(entityID);
      *transform = *t;
      transform->parent = parentID;
      if (randomPos)
        transform->position = GetRandomPosition(spawnRadius);
    } else if (auto m = dynamic_cast<Mesh*>(c)) {
      auto filter = AddComponent<MeshFilter>(entityID);
      filter->mesh = *m;
    } else if (auto m = dynamic_cast<Material*>(c)) {
      auto renderer = AddComponent<MeshRenderer>(entityID);
      renderer->material = *m;
    }
  assetManager.ForEachChild(assetID, [this, &entityID](unsigned int id) {
    auto name = assetManager.GetName(id);
    auto childID = Spawn(name, entityID);
    AddChildEntity(entityID, childID);
  });
  return entityID;
}
void Application::SpawnMulti(const std::string& name, int count, float radius) {
  for (auto i = 0; i < count; ++i) {
    auto nameTemp = name;
    Spawn(nameTemp, -1, true, radius);
  }
}
bool Application::GetKey(int key) const {
  return inputManager.GetKey(key);
}
bool Application::GetButton(int button) const {
  return inputManager.GetButton(button);
}
void Application::EnableAllKeys() {
  inputManager.EnableAllKeys();
}
void Application::DisableAllKeys() {
  inputManager.DisableAllKeys();
}
void Application::SetInputCallback(int key, int action, std::function<void()> callback, std::string description) {
  inputManager.RegisterCallback(key, action, callback, description);
}
void Application::UnsetInputCallback(int key, int action) {
  inputManager.UnregisterCallback(key, action);
}
int Application::LoadModel(const std::filesystem::path& path) {
  return assetLoader.LoadModel(path);
}
int Application::LoadPrimitive(PrimitiveID id) {
  return assetLoader.LoadPrimitive(id);
}
int Application::LoadCubeMap(std::string& name, const std::filesystem::path& top, const std::filesystem::path& bottom, const std::filesystem::path& right, const std::filesystem::path& left, const std::filesystem::path& front, const std::filesystem::path& back) {
  return assetLoader.LoadCubeMap(name, top, bottom, right, left, front, back);
}
