#define STB_IMAGE_IMPLEMENTATION
#include <app_config.hpp>
#include <application.hpp>
#include <camera.hpp>
#include <chrono>
#include <command.hpp>
#include <component.hpp>
#include <component_traits.hpp>
#include <entity_manager.hpp>
#include <filesystem>
#include <glad/glad.h>
#include <id.hpp>
#include <primitive.hpp>
#include <rendering_system.hpp>
#include <scene.hpp>
#include <spdlog/spdlog.h>
#include <stb_image.h>
#include <string>
#include <system.hpp>
#include <texture.hpp>
#include <transform.hpp>
#include <vector>
//
#include <GLFW/glfw3.h>
namespace kuki {
AppConfig::AppConfig(std::string name, std::filesystem::path logoPath, int screenWidth, int screenHeight)
  : name(name), logoPath(logoPath), screenWidth(screenWidth), screenHeight(screenHeight) {}
Application::Application(const AppConfig& config)
  : config(config), assetManager(), assetLoader(this, assetManager), inputManager(), sceneManager(), commandManager() {
  // TODO: validate the config, fallback to defaults for invalid entries
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
size_t Application::CreateScene(const std::string& name) {
  return sceneManager.Create(name);
}
void Application::DeleteScene(size_t id) {
  sceneManager.Delete(id);
}
Scene* Application::GetActiveScene() {
  return sceneManager.Get(activeSceneId);
}
void Application::SetActiveScene(size_t id) {
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
  static constexpr auto GL_MAJOR = 4;
  static constexpr auto GL_MINOR = 6;
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, GL_MAJOR);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, GL_MINOR);
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
  if (major < GL_MAJOR || (major == GL_MAJOR && minor < GL_MINOR)) {
    spdlog::error("OpenGL {}.{} or higher is required.", GL_MAJOR, GL_MINOR);
    return;
  }
  SetWindowIcon(config.logoPath);
  glfwSwapInterval(0);
  glfwSetWindowUserPointer(window, this);
  glfwSetCursorPosCallback(window, CursorPosCallback);
  glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
  glfwSetKeyCallback(window, KeyCallback);
  glfwSetCharCallback(window, CharCallback);
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
  assetManager.UpdateComponents<Transform>();
  for (auto system : systems)
    system->Update(deltaTime);
};
void Application::LateUpdate() {
  for (auto system : systems)
    system->LateUpdate(deltaTime);
  glfwSwapBuffers(window);
  glfwPollEvents();
}
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
  while (Status()) {
    Update();
    LateUpdate();
  }
  Shutdown();
}
void Application::RegisterCommand(ICommand* command) {
  commandManager.Register(command);
}
void Application::UnregisterCommand(const std::string& name) {
  commandManager.Unregister(name);
}
int Application::DispatchCommand(const std::string& input, std::string& message) {
  return commandManager.Dispatch(input, message);
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
void Application::CharCallback(GLFWwindow* window, unsigned int codepoint) {
  auto instance = static_cast<Application*>(glfwGetWindowUserPointer(window));
  if (instance)
    instance->inputManager.CharCallback(window, codepoint);
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
ID Application::CreateEntity(std::string& name) {
  auto scene = GetActiveScene();
  if (!scene)
    return ID::Invalid();
  return scene->CreateEntity(name);
}
void Application::DeleteEntity(const ID id) {
  auto scene = GetActiveScene();
  if (!scene)
    return;
  scene->DeleteEntity(id);
}
ID Application::CreateAsset(std::string& name) {
  return assetManager.Create(name);
}
void Application::DeleteAsset(ID id) {
  assetManager.Delete(id);
}
void Application::DeleteEntity(const std::string& name) {
  auto scene = GetActiveScene();
  if (!scene)
    return;
  scene->DeleteEntity(name);
}
void Application::DeleteAllEntities() {
  auto scene = GetActiveScene();
  if (!scene)
    return;
  scene->DeleteAllEntities();
}
void Application::DeleteAllEntities(const std::string& prefix) {
  auto scene = GetActiveScene();
  if (!scene)
    return;
  scene->DeleteAllEntities(prefix);
}
size_t Application::GetEntityCount() {
  auto scene = GetActiveScene();
  if (!scene)
    return 0;
  return scene->entityManager.GetCount();
}
std::string Application::GetEntityName(const ID id) {
  auto scene = GetActiveScene();
  if (!scene)
    return "";
  return scene->entityManager.GetName(id);
}
std::string Application::GetAssetName(const ID id) {
  return assetManager.GetName(id);
}
bool Application::IsEntity(const ID id) {
  auto scene = GetActiveScene();
  if (!scene)
    return false;
  return scene->entityManager.IsEntity(id);
}
ID Application::GetEntityId(const std::string& name) {
  auto scene = GetActiveScene();
  if (!scene)
    return ID::Invalid();
  return scene->entityManager.GetId(name);
}
ID Application::GetAssetId(const std::string& name) {
  return assetManager.GetId(name);
}
void Application::RenameEntity(const ID id, std::string& name) {
  auto scene = GetActiveScene();
  if (!scene)
    return;
  scene->entityManager.Rename(id, name);
}
IComponent* Application::AddEntityComponent(const ID id, const std::string& name) {
  auto scene = GetActiveScene();
  if (!scene)
    return nullptr;
  return scene->entityManager.AddComponent(id, name);
}
void Application::RemoveEntityComponent(const ID id, ComponentType type) {
  auto scene = GetActiveScene();
  if (!scene)
    return;
  scene->entityManager.RemoveComponent(id, type);
}
void Application::RemoveEntityComponent(const ID id, const std::string& name) {
  auto scene = GetActiveScene();
  if (!scene)
    return;
  scene->entityManager.RemoveComponent(id, name);
}
IComponent* Application::GetEntityComponent(const ID id, ComponentType type) {
  auto scene = GetActiveScene();
  if (!scene)
    return nullptr;
  return scene->entityManager.GetComponent(id, type);
}
IComponent* Application::GetEntityComponent(const ID id, const std::string& name) {
  auto scene = GetActiveScene();
  if (!scene)
    return nullptr;
  return scene->entityManager.GetComponent(id, name);
}
std::vector<IComponent*> Application::GetAllEntityComponents(const ID id) {
  auto scene = GetActiveScene();
  if (!scene)
    return std::vector<IComponent*>{};
  return scene->entityManager.GetAllComponents(id);
}
std::vector<std::string> Application::GetMissingEntityComponents(const ID id) {
  auto scene = GetActiveScene();
  if (!scene)
    return std::vector<std::string>{};
  return scene->entityManager.GetMissingComponents(id);
}
void Application::SortEntityTransforms() {
  auto scene = GetActiveScene();
  if (!scene)
    return;
  scene->SortTransforms();
}
void Application::UpdateEntityTransforms() {
  auto scene = GetActiveScene();
  if (!scene)
    return;
  scene->UpdateTransforms();
}
bool Application::AddChildEntity(const ID parent, const ID child) {
  auto scene = GetActiveScene();
  if (!scene)
    return false;
  return scene->entityManager.AddChild(parent, child);
}
bool Application::EntityHasParent(const ID id) {
  auto scene = GetActiveScene();
  if (!scene)
    return false;
  return scene->entityManager.HasParent(id);
}
bool Application::EntityHasChildren(const ID id) {
  auto scene = GetActiveScene();
  if (!scene)
    return false;
  return scene->entityManager.HasChildren(id);
}
size_t Application::GetFPS() {
  auto renderingSystem = GetSystem<RenderingSystem>();
  if (!renderingSystem)
    return 0;
  return renderingSystem->GetFPS();
}
bool Application::GetKey(int key) const {
  return inputManager.GetState(key);
}
bool Application::GetKeyDown(int key) const {
  return inputManager.IsPressed(key);
}
bool Application::GetKeyUp(int key) const {
  return inputManager.IsReleased(key);
}
bool Application::GetButton(int button) const {
  return inputManager.GetState(button);
}
bool Application::GetButtonDown(int button) const {
  return inputManager.IsPressed(button);
}
bool Application::GetButtonUp(int button) const {
  return inputManager.IsReleased(button);
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
void Application::EnableKeys() {
  inputManager.EnableKeys();
}
void Application::DisableKeys() {
  inputManager.DisableKeys();
}
void Application::EnableButtons() {
  inputManager.EnableButtons();
}
void Application::DisableButtons() {
  inputManager.DisableButtons();
}
void Application::EnableInputs() {
  inputManager.EnableAll();
}
void Application::DisableInputs() {
  inputManager.DisableAll();
}
void Application::RegisterInputAction(int trigger, InputAction action, bool press) {
  inputManager.RegisterAction(trigger, action, press);
}
void Application::RegisterInputAction(const std::string& trigger, InputAction action) {
  inputManager.RegisterAction(trigger, action);
}
void Application::UnregisterInputAction(int trigger, bool press) {
  inputManager.UnregisterAction(trigger, press);
}
void Application::UnregisterInputAction(const std::string& trigger) {
  inputManager.UnregisterAction(trigger);
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
ID Application::LoadPrimitive(PrimitiveType id) {
  return assetLoader.LoadPrimitive(id);
}
} // namespace kuki
