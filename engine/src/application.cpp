#include <application.hpp>
#include <application_description.hpp>
#include <camera.hpp>
#include <chrono>
#include <concepts.hpp>
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
#include <transform.hpp>
#include <vector>
//
#include <GLFW/glfw3.h>
namespace kuki {
Application::Application(ApplicationDescription desc)
  : desc(std::move(desc)) {
  desc.path = GetExePath();
}
Application::~Application() {
  Shutdown();
}
auto Application::Awake() -> void {}
auto Application::Start() -> void {}
auto Application::Update(const float) -> void {}
auto Application::Shutdown() -> void {}
auto Application::Run() -> void {
  // NOTE: Pre/Late* functions are not exposed to child classes
  PreAwake();
  Awake();
  PreStart();
  Start();
  while (Status()) {
    PreUpdate();
    Update(deltaTime);
    LateUpdate();
  }
  Shutdown();
  LateShutdown();
}
auto Application::PreAwake() -> void {
  CreateWindow();
  CreateSystem<RenderingSystem>(sceneManager, assetManager);
  for (auto &[_, system] : typeIndexToSystem)
    system->Awake();
}
auto Application::PreStart() -> void {
  for (auto &[_, system] : typeIndexToSystem)
    system->Start();
};
auto Application::PreUpdate() -> void {
  const auto timeNow = std::chrono::high_resolution_clock::now();
  static auto timeLast = timeNow;
  deltaTime = std::chrono::duration<float>(timeNow - timeLast).count();
  timeLast = timeNow;
  assetManager.Update();
  for (auto &[_, system] : typeIndexToSystem)
    system->Update(deltaTime);
};
auto Application::LateUpdate() -> void {
  glfwSwapBuffers(window);
  glfwPollEvents();
}
auto Application::LateShutdown() -> void {
  for (auto &[_, system] : typeIndexToSystem)
    system->Shutdown();
  typeIndexToSystem.clear();
  glfwDestroyWindow(window);
  glfwTerminate();
};
auto Application::Status() -> bool {
  return !glfwWindowShouldClose(window);
};
auto Application::CreateWindow() -> void {
  static constexpr auto GL_MAJOR = 4;
  static constexpr auto GL_MINOR = 6;
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, GL_MAJOR);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, GL_MINOR);
  glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
  glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_SAMPLES, 4);
  window = glfwCreateWindow(settings.width, settings.height, desc.name.c_str(), nullptr, nullptr);
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
  auto version = reinterpret_cast<const char *>(glGetString(GL_VERSION));
  spdlog::info("OpenGL version: {}", version);
  int major, minor;
  glGetIntegerv(GL_MAJOR_VERSION, &major);
  glGetIntegerv(GL_MINOR_VERSION, &minor);
  if (major < GL_MAJOR || (major == GL_MAJOR && minor < GL_MINOR)) {
    spdlog::error("OpenGL {}.{} or higher is required.", GL_MAJOR, GL_MINOR);
    return;
  }
  SetWindowIcon();
  glfwSwapInterval(0);
  glfwSetWindowUserPointer(window, this);
  glfwSetCursorPosCallback(window, CursorPosCallback);
  glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
  glfwSetKeyCallback(window, KeyCallback);
  glfwSetCharCallback(window, CharCallback);
  glfwSetMouseButtonCallback(window, MouseButtonCallback);
  glfwSetWindowCloseCallback(window, WindowCloseCallback);
  glViewport(0, 0, settings.width, settings.height);
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
auto Application::ActivateScene(const std::string &name) -> bool {
  return sceneManager.Activate(name);
}
auto Application::AddChildEntity(const EntityID parent, const EntityID child) -> bool {
  if (auto scene = GetActiveScene(); scene)
    return scene->AddChildEntity(parent, child);
  return false;
}
auto Application::AddEntityComponent(const EntityID id, const ComponentType type) -> void {
  if (auto scene = GetActiveScene(); scene)
    return scene->AddEntityComponent(id, type);
}
auto Application::CreateEntity(std::string name) -> EntityID {
  if (auto scene = GetActiveScene(); scene)
    return scene->CreateEntity(std::move(name));
  return EntityID::Invalid;
}
auto Application::CreateScene(std::string name) -> SceneID {
  return sceneManager.Create(std::move(name));
}
auto Application::DeleteEntities() -> void {
  if (auto scene = GetActiveScene(); scene)
    scene->DeleteEntities();
}
auto Application::DeleteEntity(const EntityID id) -> void {
  if (auto scene = GetActiveScene(); scene)
    scene->DeleteEntity(id);
}
auto Application::DeleteScene(const std::string &name) -> bool {
  return sceneManager.Delete(name);
}
auto Application::DisableButtons() -> void {
  inputManager.DisableButtons();
}
auto Application::DisableInputs() -> void {
  inputManager.DisableAll();
}
auto Application::DisableKeys() -> void {
  inputManager.DisableKeys();
}
auto Application::EnableButtons() -> void {
  inputManager.EnableButtons();
}
auto Application::EnableInputs() -> void {
  inputManager.EnableAll();
}
auto Application::EnableKeys() -> void {
  inputManager.EnableKeys();
}
auto Application::EntityHasChildren(const EntityID id) const -> bool {
  if (auto scene = GetActiveScene(); scene)
    return scene->EntityHasChildren(id);
  return false;
}
auto Application::EntityHasParent(const EntityID id) const -> bool {
  if (auto scene = GetActiveScene(); scene)
    return scene->EntityHasParent(id);
  return false;
}
auto Application::GetArrowKeys() const -> glm::vec2 {
  return inputManager.GetArrow();
};
auto Application::GetAssetName(const AssetID id) const -> std::string {
  return assetManager.GetName(id);
}
auto Application::GetButton(int button) const -> bool {
  return inputManager.GetState(button);
}
auto Application::GetButtonDown(int button) const -> bool {
  return inputManager.IsPressed(button);
}
auto Application::GetButtonUp(int button) const -> bool {
  return inputManager.IsReleased(button);
}
auto Application::GetEntityComponent(const EntityID id, const ComponentType type) -> std::optional<ComponentVariant> {
  if (auto scene = GetActiveScene(); scene)
    return scene->GetEntityComponent(id, type);
  return std::nullopt;
}
auto Application::GetEntityComponentTypes(const EntityID id) const -> std::vector<ComponentType> {
  if (auto scene = GetActiveScene(); scene)
    return scene->GetEntityComponentTypes(id);
  return {};
}
auto Application::GetEntityCount() const -> size_t {
  if (auto scene = GetActiveScene(); scene)
    return scene->GetEntityCount();
  return 0;
}
auto Application::GetEntityName(const EntityID id) const -> std::string {
  if (auto scene = GetActiveScene(); scene)
    return scene->GetEntityName(id);
  return "";
}
auto Application::GetFPS() const -> size_t {
  if (auto renderingSystem = GetSystem<RenderingSystem>(); renderingSystem)
    return renderingSystem->GetFPS();
  return 0;
}
auto Application::GetInfo() const -> const ApplicationDescription & {
  return desc;
}
auto Application::GetKey(int key) const -> bool {
  return inputManager.GetState(key);
}
auto Application::GetKeyDown(int key) const -> bool {
  return inputManager.IsPressed(key);
}
auto Application::GetKeyUp(int key) const -> bool {
  return inputManager.IsReleased(key);
}
auto Application::GetMissingEntityComponents(const EntityID id) const -> std::vector<ComponentType> {
  if (auto scene = GetActiveScene(); scene)
    return scene->GetMissingEntityComponents(id);
  return {};
}
auto Application::GetMousePos() const -> glm::vec2 {
  return inputManager.GetMousePos();
};
auto Application::GetName() const -> std::string {
  return desc.name;
}
auto Application::GetSettings() const -> const ApplicationSettings & {
  return settings;
}
auto Application::GetWASDKeys() const -> glm::vec2 {
  return inputManager.GetWASD();
};
auto Application::InstantiateAsset(const AssetID id) -> EntityID {
  if (auto scene = GetActiveScene(); scene)
    return assetManager.Instantiate(id, *scene);
  return EntityID::Invalid;
}
auto Application::IsEntity(const EntityID id) const -> bool {
  if (auto scene = GetActiveScene(); scene)
    return scene->IsEntity(id);
  return false;
}
auto Application::LoadScene(const std::string &name) -> bool {
  return sceneManager.Load(name);
}
auto Application::RegisterInputAction(std::string trigger, InputAction action) -> void {
  inputManager.RegisterAction(std::move(trigger), action);
}
auto Application::RegisterInputAction(int trigger, InputAction action, bool press) -> void {
  inputManager.RegisterAction(trigger, action, press);
}
auto Application::RemoveEntityComponent(const EntityID id, const ComponentType type) -> bool {
  if (auto scene = GetActiveScene(); scene)
    return scene->RemoveEntityComponent(id, type);
  return false;
}
auto Application::RenameEntity(const EntityID id, std::string name) -> bool {
  if (auto scene = GetActiveScene(); scene)
    return scene->RenameEntity(id, name);
  return false;
}
auto Application::UnloadAsset(const AssetID id) -> bool {
  return assetManager.Unload(id);
}
auto Application::UnregisterInputAction(const std::string &trigger) -> void {
  inputManager.UnregisterAction(trigger);
}
auto Application::UnregisterInputAction(int trigger, bool press) -> void {
  inputManager.UnregisterAction(trigger, press);
}
auto Application::GetExePath() -> std::filesystem::path {
  int length = wai_getExecutablePath(nullptr, 0, nullptr);
  std::string path(length, '\0');
  wai_getExecutablePath(path.data(), length, nullptr);
  return std::filesystem::path(path).parent_path();
}
auto Application::SetWindowIcon() -> void {
  if (desc.iconPath.empty())
    return;
  int width, height, channels;
  const auto iconPath = (desc.path / desc.iconPath).string();
  if (auto data = stbi_load(iconPath.c_str(), &width, &height, &channels, 4); data) {
    GLFWimage images[1]{};
    images[0].width = width;
    images[0].height = height;
    images[0].pixels = data;
    glfwSetWindowIcon(window, 1, images);
    stbi_image_free(data);
  } else
    spdlog::error("Failed to load app icon: {}", iconPath);
}
auto Application::CharCallback(GLFWwindow *window, unsigned int codepoint) -> void {
  if (auto instance = static_cast<Application *>(glfwGetWindowUserPointer(window)); instance)
    instance->inputManager.CharCallback(window, codepoint);
}
auto Application::CursorPosCallback(GLFWwindow *window, double xpos, double ypos) -> void {
  if (auto instance = static_cast<Application *>(glfwGetWindowUserPointer(window)); instance)
    instance->inputManager.CursorPosCallback(window, xpos, ypos);
}
auto Application::DebugMessageCallback(unsigned int source, unsigned int type, unsigned int id, unsigned int severity, int length, const char *message, const void *userParam) -> void {
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
auto Application::FramebufferSizeCallback(GLFWwindow *window, int width, int height) -> void {
  glViewport(0, 0, width, height);
}
auto Application::KeyCallback(GLFWwindow *window, int key, int scancode, int action, int mods) -> void {
  if (auto instance = static_cast<Application *>(glfwGetWindowUserPointer(window)); instance)
    instance->inputManager.KeyCallback(window, key, scancode, action, mods);
}
auto Application::MouseButtonCallback(GLFWwindow *window, int button, int action, int mods) -> void {
  if (auto instance = static_cast<Application *>(glfwGetWindowUserPointer(window)); instance)
    instance->inputManager.MouseButtonCallback(window, button, action, mods);
}
auto Application::WindowCloseCallback(GLFWwindow *window) -> void {
  glfwSetWindowShouldClose(window, GLFW_TRUE);
}
} // namespace kuki
