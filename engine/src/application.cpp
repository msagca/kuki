#include <application.hpp>
#include <application_description.hpp>
#include <array>
#include <camera.hpp>
#include <chrono>
#include <concepts.hpp>
#include <entity_manager.hpp>
#include <filesystem>
#include <ico.h>
#include <id.hpp>
#include <kaitai/kaitaistream.h>
#include <memory>
#include <physics_system.hpp>
#include <primitive.hpp>
#include <rendering_system.hpp>
#include <scene.hpp>
#include <scripting_system.hpp>
#include <shader_asset.hpp>
#include <spdlog/spdlog.h>
#include <stb_image.h>
#include <string>
#include <system.hpp>
#include <transform.hpp>
#include <vector>
//
#include <GLFW/glfw3.h>
#include <glad/glad.h>
namespace kuki {
Application::Application(ApplicationDescription desc)
  : desc(std::move(desc)) {
  this->desc.path = GetExePath();
}
Application::~Application() {}
auto Application::Start() -> void {}
auto Application::Update(const float) -> void {}
auto Application::Shutdown() -> void {}
auto Application::Run() -> void {
  PreStart();
  Start();
  PostStart();
  while (Status()) {
    PreUpdate();
    Update(deltaTime);
    PostUpdate();
  }
  PreShutdown();
  Shutdown();
}
auto Application::PreStart() -> void {
  if (!CreateWindow())
    return;
  LoadPrimitiveAssets();
  CreateSystem<PhysicsSystem>(sceneManager);
  CreateSystem<RenderingSystem>(sceneManager, assetManager, settingsManager);
  CreateSystem<ScriptingSystem>(*this);
  // FIXME: start systems in a specific order based on their dependencies
  for (auto &[typeIndex, system] : typeIndexToSystem)
    if (typeIndex != std::type_index(typeid(ScriptingSystem)))
      system->Start();
};
auto Application::PostStart() -> void {
  const auto typeIndex = std::type_index(typeid(ScriptingSystem));
  if (auto it = typeIndexToSystem.find(typeIndex); it != typeIndexToSystem.end())
    it->second->Start();
};
auto Application::PreUpdate() -> void {
  const auto timeNow = std::chrono::high_resolution_clock::now();
  static auto timeLast = timeNow;
  deltaTime = std::chrono::duration<float>(timeNow - timeLast).count();
  timeLast = timeNow;
  glfwPollEvents();
  assetManager.Update();
  for (auto &[_, system] : typeIndexToSystem)
    system->Update(deltaTime);
};
auto Application::PostUpdate() -> void {
  glfwSwapBuffers(window);
}
auto Application::PreShutdown() -> void {
  for (auto &[_, system] : typeIndexToSystem)
    system->Shutdown();
  typeIndexToSystem.clear();
  glfwDestroyWindow(window);
  glfwTerminate();
};
auto Application::Status() -> bool {
  return window && !glfwWindowShouldClose(window);
};
auto Application::CreateWindow() -> bool {
  static constexpr auto GL_MAJOR = 4;
  static constexpr auto GL_MINOR = 6;
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, GL_MAJOR);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, GL_MINOR);
  glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_SAMPLES, 4);
  const auto &settings = GetSettings();
  window = glfwCreateWindow(settings.res.width, settings.res.height, desc.name.c_str(), nullptr, nullptr);
  if (!window) {
    spdlog::error("[App] failed to create window.");
    glfwTerminate();
    return false;
  }
  glfwMakeContextCurrent(window);
  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    spdlog::error("[App] failed to initialize GLAD.");
    return false;
  }
  auto version = reinterpret_cast<const char *>(glGetString(GL_VERSION));
  spdlog::info("[OpenGL] version: {}", version);
  int major, minor;
  glGetIntegerv(GL_MAJOR_VERSION, &major);
  glGetIntegerv(GL_MINOR_VERSION, &minor);
  if (major < GL_MAJOR || (major == GL_MAJOR && minor < GL_MINOR)) {
    spdlog::error("[OpenG] version {}.{} or higher is required.", GL_MAJOR, GL_MINOR);
    return false;
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
  glViewport(0, 0, settings.res.width, settings.res.height);
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
  return true;
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
auto Application::DeltaTime() const -> float {
  return deltaTime;
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
  return inputManager.GetArrowKeys();
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
auto Application::GetDescription() const -> const ApplicationDescription & {
  return desc;
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
auto Application::GetMousePosition() const -> glm::vec2 {
  return inputManager.GetMousePosition();
};
auto Application::GetName() const -> std::string {
  return desc.name;
}
auto Application::GetSettings() const -> const ApplicationSettings & {
  return settingsManager.GetSettings();
}
auto Application::GetWASDKeys() const -> glm::vec2 {
  return inputManager.GetWASD();
};
auto Application::InstantiateAsset(const AssetID id) -> EntityID {
  if (auto scene = GetActiveScene(); scene)
    return assetManager.Instantiate(id, *scene);
  return EntityID::Invalid;
}
auto Application::InstantiateAsset(const std::string &name) -> EntityID {
  if (auto scene = GetActiveScene(); scene)
    return assetManager.Instantiate(name, *scene);
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
auto Application::LoadPrimitive(const std::string &name) -> void {
  if (assetManager.GetID(name))
    return;
  auto meshAsset = std::make_unique<MeshAsset>(AssetID::Generate(), name);
  if (name == "Cube")
    meshAsset->mesh.vertices = std::move(Primitive::Cube());
  else if (name == "CubeInverted") {
    meshAsset->mesh.vertices = std::move(Primitive::Cube());
    Primitive::FlipWindingOrder(meshAsset->mesh.vertices);
  } else if (name == "Frame")
    meshAsset->mesh.vertices = std::move(Primitive::Frame());
  else if (name == "Plane")
    meshAsset->mesh.vertices = std::move(Primitive::Plane());
  else if (name == "Cylinder")
    meshAsset->mesh.vertices = std::move(Primitive::Cylinder());
  else if (name == "Sphere")
    meshAsset->mesh.vertices = std::move(Primitive::Sphere());
  else {
    spdlog::warn("[App] unknown primitive: {}", name);
    return;
  }
  meshAsset->bounds = BoundingBox::Calculate(meshAsset->mesh.vertices);
  if (!assetManager.GetID("DefaultLit")) {
    auto defaultLit = std::make_unique<MaterialAsset>(AssetID::Generate(), "DefaultLit");
    defaultLit->type = MaterialType::Lit;
    assetManager.Add<MaterialAsset>(std::move(defaultLit));
  }
  meshAsset->material = assetManager.GetID("DefaultLit");
  assetManager.Add<MeshAsset>(std::move(meshAsset));
}
auto Application::PreviewAsset(const AssetID id) -> RenderTarget * {
  auto renderingSystem = GetSystem<RenderingSystem>();
  if (!renderingSystem)
    return nullptr;
  return renderingSystem->PreviewAsset(id);
}
auto Application::RegisterInputAction(const std::string &trigger, InputAction action) -> void {
  inputManager.RegisterAction(trigger, std::move(action));
}
auto Application::RegisterInputAction(int trigger, InputAction action, bool press) -> void {
  inputManager.RegisterAction(trigger, std::move(action), press);
}
auto Application::RemoveEntityComponent(const EntityID id, const ComponentType type) -> bool {
  if (auto scene = GetActiveScene(); scene)
    return scene->RemoveEntityComponent(id, type);
  return false;
}
auto Application::RenameEntity(const EntityID id, std::string name) -> bool {
  if (auto scene = GetActiveScene(); scene)
    return scene->RenameEntity(id, std::move(name));
  return false;
}
auto Application::SetResolution(const int width, const int height) -> void {
  settingsManager.SetResolution(width, height);
}
auto Application::GetExePath() -> std::filesystem::path {
  int length = wai_getExecutablePath(nullptr, 0, nullptr);
  std::string path(length, '\0');
  wai_getExecutablePath(path.data(), length, nullptr);
  return std::filesystem::path(path).parent_path();
}
auto Application::LoadPrimitiveAssets() -> void {
  LoadPrimitive("Cube");
  LoadPrimitive("CubeInverted");
  LoadPrimitive("Cylinder");
  LoadPrimitive("Frame");
  LoadPrimitive("Plane");
  LoadPrimitive("Sphere");
}
auto Application::SetWindowIcon() -> void {
  if (desc.iconPath.empty())
    return;
  const auto path = (desc.path / desc.iconPath).string();
  const auto ext = desc.iconPath.extension();
  if (ext == ".ico") {
    std::ifstream is(path, std::ifstream::binary);
    kaitai::kstream ks(&is);
    ico_t data(&ks);
    if (data.num_images() == 0) {
      spdlog::error("[App] failed to load icon: {}", path);
      return;
    }
    auto iMax = 0;
    for (int i = 0, wMax = 0; i < data.num_images(); ++i)
      if (auto w = data.images()->at(i)->width(); w > wMax) {
        iMax = i;
        wMax = w;
      }
    auto &imgPtr = data.images()->at(iMax);
    auto &&imgStr = imgPtr->img();
    auto &&pixels = reinterpret_cast<unsigned char *>(imgStr.data());
    if (imgPtr->is_png()) {
      int width, height, channels;
      if (auto data = stbi_load_from_memory(pixels, imgStr.size(), &width, &height, &channels, 4); data) {
        std::array<GLFWimage, 1> images{width, height, data};
        glfwSetWindowIcon(window, 1, images.data());
        stbi_image_free(data);
        spdlog::info("[App] loaded icon: {}", path);
      } else
        spdlog::error("[App] failed to load icon: {}", path);
    } else {
      pixels += 40; // skip the DIP (BMP) header
      const auto width = imgPtr->width();
      const auto height = imgPtr->height();
      const auto rowSize = width * 4;
      auto data = std::make_unique<unsigned char[]>(width * height * 4);
      for (auto y = 0; y < height; ++y) // flip the image
        memcpy(&data[y * rowSize], &pixels[(height - 1 - y) * rowSize], rowSize);
      std::array<GLFWimage, 1> images{width, height, data.release()};
      glfwSetWindowIcon(window, 1, images.data());
      spdlog::info("[App] loaded icon: {}", path);
    }
  } else {
    int width, height, channels;
    if (auto data = stbi_load(path.c_str(), &width, &height, &channels, 4); data) {
      std::array<GLFWimage, 1> images{width, height, data};
      glfwSetWindowIcon(window, 1, images.data());
      stbi_image_free(data);
      spdlog::info("[App] loaded icon: {}", path);
    } else
      spdlog::error("[App] failed to load icon: {}", path);
  }
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
    sourceStr = "window system";
    break;
  case GL_DEBUG_SOURCE_SHADER_COMPILER:
    sourceStr = "shader compiler";
    break;
  case GL_DEBUG_SOURCE_THIRD_PARTY:
    sourceStr = "third party";
    break;
  case GL_DEBUG_SOURCE_APPLICATION:
    sourceStr = "application";
    break;
  default:
    sourceStr = "other";
    break;
  }
  switch (type) {
  case GL_DEBUG_TYPE_ERROR:
    typeStr = "error";
    break;
  case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
    typeStr = "deprecated behavior";
    break;
  case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
    typeStr = "undefined behavior";
    break;
  case GL_DEBUG_TYPE_PORTABILITY:
    typeStr = "portability";
    break;
  case GL_DEBUG_TYPE_PERFORMANCE:
    typeStr = "performance";
    break;
  case GL_DEBUG_TYPE_MARKER:
    typeStr = "marker";
    break;
  case GL_DEBUG_TYPE_PUSH_GROUP:
    typeStr = "push group";
    break;
  case GL_DEBUG_TYPE_POP_GROUP:
    typeStr = "pop group";
    break;
  default:
    typeStr = "other";
    break;
  }
  switch (severity) {
  case GL_DEBUG_SEVERITY_HIGH:
    spdlog::error("[OpenGL] {} {}: {}", sourceStr, typeStr, message);
    break;
  case GL_DEBUG_SEVERITY_MEDIUM:
    spdlog::warn("[OpenGL] {} {}: {}", sourceStr, typeStr, message);
    break;
  case GL_DEBUG_SEVERITY_LOW:
    spdlog::info("[OpenGL] {} {}: {}", sourceStr, typeStr, message);
    break;
  default:
    spdlog::debug("[OpenGL] {} {}: {}", sourceStr, typeStr, message);
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
