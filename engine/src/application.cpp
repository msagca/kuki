#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <application.hpp>
#include <application_description.hpp>
#include <array>
#include <asset_type.hpp>
#include <bounding_box.hpp>
#include <chrono>
#include <component.hpp>
#include <component_type.hpp>
#include <concepts.hpp>
#include <filesystem>
#include <fstream>
#include <glad/glad.h>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_int2.hpp>
#include <ico.h>
#include <id.hpp>
#include <kaitai/kaitaistream.h>
#include <material_asset.hpp>
#include <material_type.hpp>
#include <memory>
#include <mesh_asset.hpp>
#include <model_asset.hpp>
#include <optional>
#include <primitive.hpp>
#include <render_target.hpp>
#include <rendering_system.hpp>
#include <scene.hpp>
#include <spdlog/spdlog.h>
#include <stb_image.h>
#include <string.h>
#include <string>
#include <system.hpp>
#include <trie.hpp>
#include <typeindex>
#include <utility>
#include <vector>
#include <whereami.h>
namespace kuki {
Application::Application(ApplicationDescription desc)
  : desc(std::move(desc)), assetManager(*this), inputManager(*this), sceneManager(*this) {
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
  StartSystems();
};
auto Application::PostStart() -> void {
  PostStartSystems();
};
auto Application::PreUpdate() -> void {
  const auto timeNow = std::chrono::high_resolution_clock::now();
  static auto timeLast = timeNow;
  deltaTime = std::chrono::duration<float>(timeNow - timeLast).count();
  timeLast = timeNow;
  inputManager.ResetScroll();
  inputManager.ResetPulses();
  glfwPollEvents();
  assetManager.Update();
  UpdateSystems(deltaTime);
};
auto Application::PostUpdate() -> void {
  glfwSwapBuffers(window);
}
auto Application::PreShutdown() -> void {
  ShutdownSystems();
  glfwDestroyWindow(window);
  glfwTerminate();
};
auto Application::Status() -> bool {
  return window && !glfwWindowShouldClose(window);
};
auto Application::CreateWindow() -> bool {
  static constexpr auto GL_MAJOR = 4;
  static constexpr auto GL_MINOR = 6;
  constexpr auto SCREEN_WIDTH = 1920;
  constexpr auto SCREEN_HEIGHT = 1080;
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, GL_MAJOR);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, GL_MINOR);
  glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_SAMPLES, 4);
  window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, desc.name.c_str(), nullptr, nullptr);
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
  glfwSetScrollCallback(window, ScrollCallback);
  glfwSetWindowCloseCallback(window, WindowCloseCallback);
  glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
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
auto Application::AddChildEntity(const EntityID parent, const EntityID child) -> bool {
  if (auto scene = GetScene(); scene)
    return scene->AddChildEntity(parent, child);
  return false;
}
auto Application::AlignView(const EntityID id) -> void {
  auto scene = sceneManager.Get();
  if (!scene)
    return;
  scene->AlignView(id);
}
auto Application::CreateEntity(std::string name) -> EntityID {
  if (auto scene = GetScene(); scene)
    return scene->CreateEntity(std::move(name));
  return EntityID::Invalid;
}
auto Application::CreateScene(std::string name) -> SceneID {
  return sceneManager.Create(std::move(name));
}
auto Application::DeleteEntities() -> void {
  if (auto scene = GetScene(); scene)
    scene->DeleteEntities();
}
auto Application::DeleteEntity(const EntityID id) -> void {
  if (auto scene = GetScene(); scene)
    scene->DeleteEntity(id);
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
  if (auto scene = GetScene(); scene)
    return scene->EntityHasChildren(id);
  return false;
}
auto Application::EntityHasParent(const EntityID id) const -> bool {
  if (auto scene = GetScene(); scene)
    return scene->EntityHasParent(id);
  return false;
}
auto Application::GetArrowKeys() const -> glm::ivec2 {
  return inputManager.GetArrowKeys();
};
auto Application::GetAssetName(const AssetID id) const -> std::string {
  return assetManager.GetName(id);
}
auto Application::GetAssetPath(const AssetID id) const -> std::filesystem::path {
  return assetManager.GetPath(id);
}
auto Application::GetAssetType(const AssetID id) const -> AssetType {
  return assetManager.GetType(id);
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
  if (auto scene = GetScene(); scene)
    return scene->GetEntityComponent(id, type);
  return std::nullopt;
}
auto Application::GetEntityComponentTypes(const EntityID id) const -> std::vector<ComponentType> {
  if (auto scene = GetScene(); scene)
    return scene->GetEntityComponentTypes(id);
  return {};
}
auto Application::GetEntityCount() const -> size_t {
  if (auto scene = GetScene(); scene)
    return scene->GetEntityCount();
  return 0;
}
auto Application::GetEntityName(const EntityID id) const -> std::string {
  if (auto scene = GetScene(); scene)
    return scene->GetEntityName(id);
  return "";
}
auto Application::GetEntityParent(const EntityID id) const -> EntityID {
  if (auto scene = GetScene(); scene)
    return scene->GetParent(id);
  return EntityID::Invalid;
}
auto Application::GetFPS() -> size_t {
  if (auto renderingSystem = GetRenderingSystem(); renderingSystem)
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
  if (auto scene = GetScene(); scene)
    return scene->GetMissingEntityComponents(id);
  return {};
}
auto Application::GetMousePosition() const -> glm::vec2 {
  return inputManager.GetMousePosition();
};
auto Application::GetName() const -> std::string {
  return desc.name;
}
auto Application::GetScrollOffset() const -> glm::vec2 {
  return inputManager.GetScrollOffset();
}
auto Application::GetWASDKeys() const -> glm::ivec2 {
  return inputManager.GetWASD();
};
auto Application::InstantiateAsset(const AssetID id) -> EntityID {
  if (auto scene = GetScene(); scene)
    return assetManager.Instantiate(id, *scene);
  return EntityID::Invalid;
}
auto Application::InstantiateAsset(const std::string &name) -> EntityID {
  if (auto scene = GetScene(); scene)
    return assetManager.Instantiate(name, *scene);
  return EntityID::Invalid;
}
auto Application::IsAssetLoaded(const AssetID id) const -> bool {
  return assetManager.IsLoaded(id);
}
auto Application::ResolveModelInstance(const EntityID root, const AssetID modelAssetId) -> void {
  auto scene = GetScene();
  if (!scene)
    return;
  if (auto modelAsset = GetAsset<ModelAsset>(modelAssetId); modelAsset)
    assetManager.ResolveModelInstance(*scene, root, *modelAsset, modelAssetId);
}
auto Application::IsEntity(const EntityID id) const -> bool {
  if (auto scene = GetScene(); scene)
    return scene->IsEntity(id);
  return false;
}
auto Application::IsViewportHovered() const -> bool {
  return viewportHovered;
}
auto Application::LoadComputeFromSource(const std::string_view source, std::string name) -> AssetID {
  return assetManager.LoadComputeFromSource(source, std::move(name));
}
auto Application::LoadShader(const std::filesystem::path &vertPath, const std::filesystem::path &fragPath, std::string name, const MaterialType materialType) -> AssetID {
  return assetManager.LoadShader(vertPath, fragPath, std::move(name), materialType);
}
auto Application::LoadShaderFromSource(const std::string_view vertSource, const std::string_view fragSource, std::string name, const MaterialType materialType) -> AssetID {
  return assetManager.LoadShaderFromSource(vertSource, fragSource, std::move(name), materialType);
}
auto Application::LoadPrimitive(const std::string &name) -> void {
  if (assetManager.Get(name))
    return;
  auto meshAsset = std::make_unique<MeshAsset>(AssetID::Generate());
  if (name == "Cube")
    meshAsset->mesh.vertices = Primitive::Cube();
  else if (name == "CubeInverted") {
    meshAsset->mesh.vertices = Primitive::Cube();
    Primitive::FlipWindingOrder(meshAsset->mesh.vertices);
  } else if (name == "Frame")
    meshAsset->mesh.vertices = Primitive::Frame();
  else if (name == "Plane")
    meshAsset->mesh.vertices = Primitive::Plane();
  else if (name == "Cylinder")
    meshAsset->mesh.vertices = Primitive::Cylinder();
  else if (name == "Sphere")
    meshAsset->mesh.vertices = Primitive::Sphere();
  else {
    spdlog::warn("[App] unknown primitive: {}", name);
    return;
  }
  meshAsset->bounds = BoundingBox::Calculate(meshAsset->mesh.vertices);
  if (!assetManager.Get("DefaultLit")) {
    auto defaultLit = std::make_unique<MaterialAsset>(AssetID::Generate());
    defaultLit->type = MaterialType::Lit;
    assetManager.Add(std::move(defaultLit), "DefaultLit");
  }
  if (auto defaultLit = assetManager.Get<MaterialAsset>("DefaultLit"); defaultLit)
    meshAsset->material = defaultLit->id;
  assetManager.Add(std::move(meshAsset), name);
}
auto Application::GetPreviewSize() -> int {
  if (auto renderingSystem = GetRenderingSystem(); renderingSystem)
    return renderingSystem->GetPreviewSize();
  return 0;
}
auto Application::PreviewAsset(const AssetID id) -> RenderTarget * {
  if (auto renderingSystem = GetRenderingSystem(); renderingSystem)
    return renderingSystem->PreviewAsset(id);
  return nullptr;
}
auto Application::SetPreviewSize(const int size) -> void {
  if (auto renderingSystem = GetRenderingSystem(); renderingSystem)
    renderingSystem->SetPreviewSize(size);
}
auto Application::BeginKeyCapture() -> void {
  inputManager.BeginKeyCapture();
}
auto Application::CancelKeyCapture() -> void {
  inputManager.CancelKeyCapture();
}
auto Application::GetBinding(const std::string &name) const -> InputManager::Trigger {
  return inputManager.GetBinding(name);
}
auto Application::GetBindingDescription(const std::string &name) const -> std::string {
  return inputManager.GetBindingDescription(name);
}
auto Application::GetBindingNames() const -> const std::vector<std::string> & {
  return inputManager.GetBindingNames();
}
auto Application::GetSequenceDescription(const std::string &sequence) const -> std::string {
  return inputManager.GetSequenceDescription(sequence);
}
auto Application::GetSequenceNames() const -> const std::vector<std::string> & {
  return inputManager.GetSequenceNames();
}
auto Application::GetTriggerName(const InputManager::Trigger &trigger) const -> std::string {
  return inputManager.GetTriggerName(trigger);
}
auto Application::IsBindingHeld(const std::string &name) const -> bool {
  return inputManager.IsBindingHeld(name);
}
auto Application::IsBindingPressed(const std::string &name) const -> bool {
  return inputManager.IsBindingPressed(name);
}
auto Application::IsSequenceInProgress() const -> bool {
  return inputManager.IsSequenceInProgress();
}
auto Application::PollKeyCapture() -> InputManager::CaptureOutcome {
  return inputManager.PollKeyCapture();
}
auto Application::RegisterBinding(const std::string &name, const InputManager::Trigger &trigger, std::string description) -> void {
  inputManager.RegisterBinding(name, trigger, std::move(description));
}
auto Application::RegisterInputAction(const std::string &trigger, InputAction action, std::string description) -> InputManager::ActionID {
  return inputManager.RegisterAction(trigger, std::move(action), std::move(description));
}
auto Application::RegisterInputAction(int trigger, InputAction action, bool press) -> InputManager::ActionID {
  return inputManager.RegisterAction(trigger, std::move(action), press);
}
auto Application::RemoveEntityScript(const EntityID id, const std::type_index type) -> bool {
  if (auto scene = GetScene(); scene)
    return scene->RemoveEntityScript(id, type);
  return false;
}
auto Application::RenameAsset(const AssetID id, std::string name) -> bool {
  return assetManager.Rename(id, std::move(name));
}
auto Application::RenameEntity(const EntityID id, std::string name) -> bool {
  if (auto scene = GetScene(); scene)
    return scene->RenameEntity(id, std::move(name));
  return false;
}
auto Application::SetActiveCamera(const EntityID id, const std::string &sceneName) -> bool {
  if (auto scene = GetScene(sceneName); scene)
    return scene->SetActiveCamera(id);
  return false;
}
auto Application::SetBinding(const std::string &name, const InputManager::Trigger &trigger) -> void {
  inputManager.SetBinding(name, trigger);
}
auto Application::SetCursorLocked(bool locked) -> void {
  glfwSetInputMode(window, GLFW_CURSOR, locked ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
  if (!locked) {
    double x, y;
    glfwGetCursorPos(window, &x, &y);
    inputManager.CursorPosCallback(window, x, y);
  }
}
auto Application::SetViewportHovered(bool hovered) -> void {
  viewportHovered = hovered;
}
auto Application::SetResolution(const int width, const int height) -> void {
  if (auto renderingSystem = GetRenderingSystem(); renderingSystem)
    renderingSystem->SetResolution(width, height);
}
auto Application::UnregisterInputAction(const InputManager::ActionID id) -> bool {
  return inputManager.UnregisterAction(id);
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
      pixels += 40;
      const auto width = imgPtr->width();
      const auto height = imgPtr->height();
      const auto rowSize = width * 4;
      auto data = std::make_unique<unsigned char[]>(width * height * 4);
      for (auto y = 0; y < height; ++y)
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
auto Application::FramebufferSizeCallback(GLFWwindow *, int width, int height) -> void {
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
auto Application::ScrollCallback(GLFWwindow *window, double xoffset, double yoffset) -> void {
  if (auto instance = static_cast<Application *>(glfwGetWindowUserPointer(window)); instance)
    instance->inputManager.ScrollCallback(window, xoffset, yoffset);
}
auto Application::WindowCloseCallback(GLFWwindow *window) -> void {
  glfwSetWindowShouldClose(window, GLFW_TRUE);
}
} // namespace kuki
