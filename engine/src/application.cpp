#define STB_IMAGE_IMPLEMENTATION
#include <system/rendering.hpp>
#include <app_config.hpp>
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
#include <component/texture.hpp>
#include <component/transform.hpp>
#include <entity_manager.hpp>
#include <filesystem>
#include <functional>
#include <GLFW/glfw3.h>
#include <glm/ext/vector_float3.hpp>
#include <primitive.hpp>
#include <random>
#include <scene.hpp>
#include <spdlog/spdlog.h>
#include <stb_image.h>
#include <string>
#include <system/system.hpp>
#include <vector>
#include <glm/ext/matrix_float4x4.hpp>
namespace kuki {
AppConfig::AppConfig(std::string name, std::filesystem::path logoPath, int screenWidth, int screenHeight)
  : name(name), logoPath(logoPath), screenWidth(screenWidth), screenHeight(screenHeight) {}
Application::Application(const AppConfig& config)
  : config(config), assetManager(), assetLoader(this, assetManager), inputManager(), sceneManager(), commandManager() {
  // TODO: check the config for illegal values
}
const std::string& Application::GetName() const {
  return config.name;
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
  return sceneManager.Get(activeSceneId);
}
void Application::SetActiveScene(unsigned int id) {
  if (sceneManager.Has(id))
    activeSceneId = id;
}
Camera* Application::GetActiveCamera() {
  auto scene = GetActiveScene();
  if (!scene)
    return nullptr;
  return scene->GetCamera();
}
void Application::Init() {
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
  glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_SAMPLES, 4);
  window = glfwCreateWindow(config.screenWidth, config.screenHeight, config.name.c_str(), nullptr, nullptr);
  if (!window) {
    spdlog::error("Failed to create GLFW window.");
    glfwTerminate();
    return;
  }
  glfwMakeContextCurrent(window);
  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    spdlog::error("Failed to initialize GLAD.");
    return;
  }
  auto version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
  spdlog::info("OpenGL version: {}", version);
  int major, minor;
  glGetIntegerv(GL_MAJOR_VERSION, &major);
  glGetIntegerv(GL_MINOR_VERSION, &minor);
  if (major < 4 || (major == 4 && minor < 6)) {
    spdlog::error("OpenGL 4.6 or higher is required.");
    return;
  }
  SetWindowIcon(config.logoPath);
  glfwSwapInterval(0);
  glfwSetWindowUserPointer(window, this);
  glfwSetCursorPosCallback(window, CursorPosCallback);
  glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
  glfwSetKeyCallback(window, KeyCallback);
  glfwSetMouseButtonCallback(window, MouseButtonCallback);
  glfwSetWindowCloseCallback(window, WindowCloseCallback);
  glViewport(0, 0, config.screenWidth, config.screenHeight);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glCullFace(GL_BACK);
  glEnable(GL_BLEND);
  glEnable(GL_CULL_FACE);
  glEnable(GL_DEBUG_OUTPUT);
  glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_MULTISAMPLE);
  glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
  glFrontFace(GL_CCW);
  glDebugMessageCallback(DebugMessageCallback, nullptr);
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
  assetLoader.Update();
  for (auto system : systems)
    system->Update(deltaTime);
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
void Application::DebugMessageCallback(unsigned int source, unsigned int type, unsigned int id, unsigned int severity, int length, const char* message, const void* userParam) {
  std::string sourceStr, typeStr;
  switch (source) {
  case GL_DEBUG_SOURCE_API:
    sourceStr = "API";
    break;
  case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
    sourceStr = "Window System";
    break;
  case GL_DEBUG_SOURCE_SHADER_COMPILER:
    sourceStr = "Shader Compiler";
    break;
  case GL_DEBUG_SOURCE_THIRD_PARTY:
    sourceStr = "Third Party";
    break;
  case GL_DEBUG_SOURCE_APPLICATION:
    sourceStr = "Application";
    break;
  case GL_DEBUG_SOURCE_OTHER:
    sourceStr = "Other";
    break;
  }
  switch (type) {
  case GL_DEBUG_TYPE_ERROR:
    typeStr = "Error";
    break;
  case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
    typeStr = "Deprecated Behavior";
    break;
  case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
    typeStr = "Undefined Behavior";
    break;
  case GL_DEBUG_TYPE_PORTABILITY:
    typeStr = "Portability";
    break;
  case GL_DEBUG_TYPE_PERFORMANCE:
    typeStr = "Performance";
    break;
  case GL_DEBUG_TYPE_MARKER:
    typeStr = "Marker";
    break;
  case GL_DEBUG_TYPE_PUSH_GROUP:
    typeStr = "Push Group";
    break;
  case GL_DEBUG_TYPE_POP_GROUP:
    typeStr = "Pop Group";
    break;
  case GL_DEBUG_TYPE_OTHER:
    typeStr = "Other";
    break;
  }
  switch (severity) {
  case GL_DEBUG_SEVERITY_HIGH:
    spdlog::error("OpenGL {} {}: {}", sourceStr, typeStr, message);
    break;
  case GL_DEBUG_SEVERITY_MEDIUM:
    spdlog::warn("OpenGL {} {}: {}", sourceStr, typeStr, message);
    break;
  case GL_DEBUG_SEVERITY_LOW:
    spdlog::info("OpenGL {} {}: {}", sourceStr, typeStr, message);
    break;
  default:
    spdlog::debug("OpenGL {} {}: {}", sourceStr, typeStr, message);
  }
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
    spdlog::error("Failed to load app icon: '{}'.", path.string());
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
int Application::CreateAsset(std::string& name) {
  return assetManager.Create(name);
}
void Application::DeleteAsset(unsigned int id) {
  assetManager.Delete(id);
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
size_t Application::GetEntityCount() {
  auto scene = GetActiveScene();
  if (!scene)
    return 0;
  return scene->GetEntityManager().GetCount();
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
bool Application::IsEntity(unsigned int id) {
  auto scene = GetActiveScene();
  if (!scene)
    return false;
  return scene->GetEntityManager().IsEntity(id);
}
int Application::GetEntityId(const std::string& name) {
  auto scene = GetActiveScene();
  if (!scene)
    return -1;
  return scene->GetEntityManager().GetId(name);
}
int Application::GetAssetId(const std::string& name) {
  return assetManager.GetId(name);
}
void Application::RenameEntity(unsigned int id, std::string& name) {
  auto scene = GetActiveScene();
  if (!scene)
    return;
  scene->GetEntityManager().Rename(id, name);
}
IComponent* Application::AddEntityComponent(unsigned int id, const std::string& name) {
  auto scene = GetActiveScene();
  if (!scene)
    return nullptr;
  return scene->GetEntityManager().AddComponent(id, name);
}
void Application::RemoveEntityComponent(unsigned int id, const std::string& name) {
  auto scene = GetActiveScene();
  if (!scene)
    return;
  scene->GetEntityManager().RemoveComponent(id, name);
}
IComponent* Application::GetEntityComponent(unsigned int id, const std::string& name) {
  auto scene = GetActiveScene();
  if (!scene)
    return nullptr;
  return scene->GetEntityManager().GetComponent(id, name);
}
std::vector<IComponent*> Application::GetAllEntityComponents(unsigned int id) {
  auto scene = GetActiveScene();
  if (!scene)
    return std::vector<IComponent*>{};
  return scene->GetEntityManager().GetAllComponents(id);
}
std::vector<std::string> Application::GetMissingEntityComponents(unsigned int id) {
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
int Application::Spawn(std::string& name, int parentId, bool randomPos, float spawnRadius) {
  // TODO: this shouldn't be a part of the Application class
  auto assetId = assetManager.GetId(name);
  if (assetId < 0)
    return -1;
  auto entityId = CreateEntity(name);
  auto [assetTransform, mesh, material] = assetManager.GetComponents<Transform, Mesh, Material>(assetId);
  if (assetTransform) {
    auto entityTransform = AddEntityComponent<Transform>(entityId);
    *entityTransform = *assetTransform;
    entityTransform->parent = parentId;
    if (randomPos)
      entityTransform->position = GetRandomPosition(spawnRadius);
  }
  if (mesh) {
    auto filter = AddEntityComponent<MeshFilter>(entityId);
    filter->mesh = *mesh;
  }
  if (material) {
    auto renderer = AddEntityComponent<MeshRenderer>(entityId);
    renderer->material = *material;
  }
  ForEachChildAsset(assetId, [this, &entityId](unsigned int childAssetId) {
    auto name = assetManager.GetName(childAssetId);
    auto childEntityId = Spawn(name, entityId, false);
    AddChildEntity(entityId, childEntityId);
  });
  return entityId;
}
void Application::SpawnMulti(const std::string& name, int count, float radius) {
  for (auto i = 0; i < count; ++i) {
    auto nameTemp = name;
    Spawn(nameTemp, -1, true, radius);
  }
}
int Application::GetFPS() {
  auto renderingSystem = GetSystem<RenderingSystem>();
  if (!renderingSystem)
    return 0;
  return renderingSystem->GetFPS();
}
bool Application::GetKeyDown(int key) const {
  return inputManager.GetKeyDown(key);
}
bool Application::GetButtonDown(int button) const {
  return inputManager.GetButtonDown(button);
}
bool Application::GetKeyUp(int key) const {
  return inputManager.GetKeyUp(key);
}
bool Application::GetButtonUp(int button) const {
  return inputManager.GetButtonUp(button);
}
glm::vec2 Application::GetMousePos() const {
  return inputManager.GetMousePos();
};
glm::vec2 Application::GetWASDKeys() const {
  return inputManager.GetWASD();
};
glm::vec2 Application::GetArrowKeys() const {
  return inputManager.GetArrow();
};
void Application::EnableAllKeys() {
  inputManager.EnableAllKeys();
}
void Application::DisableAllKeys() {
  inputManager.DisableAllKeys();
}
void Application::RegisterInputCallback(int key, int action, std::function<void()> callback, std::string description) {
  inputManager.RegisterCallback(key, action, callback, description);
}
void Application::UnregisterInputCallback(int key, int action) {
  inputManager.UnregisterCallback(key, action);
}
void Application::UpdateEntityWorldTransform(unsigned int id) {
  auto scene = GetActiveScene();
  if (!scene)
    return;
  scene->GetEntityManager().UpdateWorldTransform(id);
}
void Application::UpdateAssetWorldTransform(unsigned int id) {
  assetManager.UpdateWorldTransform(id);
}
Texture Application::CreateCubeMapFromEquirect(Texture equirect) {
  auto renderingSystem = GetSystem<RenderingSystem>();
  if (!renderingSystem)
    return Texture{};
  return renderingSystem->CreateCubeMapFromEquirect(equirect);
}
Texture Application::CreateIrradianceMapFromCubeMap(Texture cubeMap) {
  auto renderingSystem = GetSystem<RenderingSystem>();
  if (!renderingSystem)
    return Texture{};
  return renderingSystem->CreateIrradianceMapFromCubeMap(cubeMap);
}
Texture Application::CreatePrefilterMapFromCubeMap(Texture cubeMap) {
  auto renderingSystem = GetSystem<RenderingSystem>();
  if (!renderingSystem)
    return Texture{};
  return renderingSystem->CreatePrefilterMapFromCubeMap(cubeMap);
}
Texture Application::CreateBRDF_LUT() {
  auto renderingSystem = GetSystem<RenderingSystem>();
  if (!renderingSystem)
    return Texture{};
  return renderingSystem->CreateBRDF_LUT();
}
void Application::LoadModelAsync(const std::filesystem::path& path) {
  assetLoader.LoadModelAsync(path);
}
void Application::LoadTextureAsync(const std::filesystem::path& path, TextureType type) {
  assetLoader.LoadTextureAsync(path, type);
}
int Application::LoadPrimitive(PrimitiveType id) {
  return assetLoader.LoadPrimitive(id);
}
} // namespace kuki
