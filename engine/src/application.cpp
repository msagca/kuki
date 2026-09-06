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
#include <game_builder.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_int2.hpp>
#include <graphics_context.hpp>
#include <ico.h>
#include <id.hpp>
#include <kaitai/kaitaistream.h>
#include <launch_path.hpp>
#include <material_asset.hpp>
#include <material_type.hpp>
#include <memory>
#include <mesh_asset.hpp>
#include <model_asset.hpp>
#include <optional>
#include <primitive.hpp>
#include <profiler.hpp>
#include <render_target.hpp>
#include <rendering_system.hpp>
#include <scene.hpp>
#include <cstdlib>
#include <spdlog/spdlog.h>
#include <stb_image.h>
#include <string.h>
#include <string>
#include <system.hpp>
#include <trie.hpp>
#include <typeindex>
#include <utility>
#include <vector>
namespace {
/// @brief Name of the environment variable that overrides how much is logged.
constexpr auto LOG_LEVEL_VARIABLE = "KUKI_LOG";
/// @brief Decides how much this run logs, before anything has had a chance to log.
///
/// A release build says nothing at all. The logs describe what the engine is doing to whoever is
/// working on it, and a console full of asset paths is not something to show someone playing a
/// game -- the same judgement that hides the console window itself in release builds, which
/// `kuki_hide_console` does at link time.
///
/// A debug build says everything, `debug` included. Those messages were unreachable before this:
/// spdlog's own default is `info`, so every `spdlog::debug` in the engine was compiled in and
/// dropped at run time whatever the build.
///
/// `KUKI_LOG` overrides both, which is what keeps a silent release from being an undiagnosable
/// one -- a crash that only happens in a release build is exactly the crash that needs the log,
/// and rebuilding as debug is both slow and liable to make it stop happening. It takes any spdlog
/// level name (`trace`, `debug`, `info`, `warn`, `error`, `critical`, `off`); anything else is
/// read as a request to turn logging on rather than as a level, since that is what someone
/// setting `KUKI_LOG=1` means.
auto ConfigureLogging() -> void {
  const auto level = [] {
    const auto *requested = std::getenv(LOG_LEVEL_VARIABLE);
    if (!requested || !*requested) {
#ifdef NDEBUG
      return spdlog::level::off;
#else
      return spdlog::level::debug;
#endif
    }
    // `from_str` answers `off` for anything it does not recognise, which would silence a run that
    // asked to be noisy, so the unrecognised case is separated out rather than trusted.
    const std::string name(requested);
    const auto parsed = spdlog::level::from_str(name);
    return parsed == spdlog::level::off && name != "off" ? spdlog::level::debug : parsed;
  }();
  spdlog::set_level(level);
}
} // namespace
namespace kuki {
Application::Application(ApplicationDescription desc)
  : desc(std::move(desc)), assetManager(*this), inputManager(*this), sceneManager(*this) {
  // First, so that nothing this constructor goes on to do can log before the level is settled.
  ConfigureLogging();
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
  // Before the window, so that even a launch that fails to get one has read them -- and before
  // `Start`, which is where an application picks up whatever it saved last time.
  preferences.Load(desc.name);
  if (!CreateWindow())
    return;
  LoadPrimitiveAssets();
  StartSystems();
};
auto Application::PostStart() -> void {
  PostStartSystems();
};
auto Application::PreUpdate() -> void {
  Profiler::Get().BeginFrame();
  const auto timeNow = std::chrono::high_resolution_clock::now();
  static auto timeLast = timeNow;
  deltaTime = std::chrono::duration<float>(timeNow - timeLast).count();
  timeLast = timeNow;
  inputManager.ResetScroll();
  inputManager.ResetPulses();
  {
    KUKI_PROFILE_SCOPE("Input");
    glfwPollEvents();
  }
  {
    KUKI_PROFILE_SCOPE("Assets");
    assetManager.Update();
  }
  if (graphicsContext) {
    KUKI_PROFILE_SCOPE("BeginFrame");
    graphicsContext->BeginFrame();
  }
  {
    KUKI_PROFILE_SCOPE("Systems");
    UpdateSystems(deltaTime);
  }
};
auto Application::PostUpdate() -> void {
  if (graphicsContext) {
    KUKI_PROFILE_SCOPE("Present");
    graphicsContext->Present();
  }
  Profiler::Get().EndFrame();
}
auto Application::PreShutdown() -> void {
  ShutdownSystems();
  // After the systems, so a script that writes a preference as it stops is not writing into a file
  // that has already been saved.
  preferences.Save();
  if (graphicsContext)
    graphicsContext->Shutdown();
  glfwDestroyWindow(window);
  glfwTerminate();
};
auto Application::Quit() -> void {
  if (window)
    glfwSetWindowShouldClose(window, GLFW_TRUE);
}
auto Application::Status() -> bool {
  return window && !glfwWindowShouldClose(window);
};
auto Application::CreateWindow() -> bool {
  graphicsContext = GraphicsContext::Create(desc.api);
  if (!graphicsContext) {
    spdlog::error("[App] Failed to create graphics context");
    return false;
  }
  glfwInit();
  graphicsContext->ApplyWindowHints();
  auto width = desc.width;
  auto height = desc.height;
  GLFWmonitor *monitor{};
  if (desc.fullscreen) {
    // Borderless rather than exclusive: the window is created at the monitor's current mode with
    // the decorations off and no monitor handed to GLFW, so there is no mode switch to pay for on
    // every alt-tab. See `ApplicationDescription::fullscreen`.
    if (auto *primary = glfwGetPrimaryMonitor(); primary)
      if (const auto *mode = glfwGetVideoMode(primary); mode) {
        width = mode->width;
        height = mode->height;
        glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
        glfwWindowHint(GLFW_RED_BITS, mode->redBits);
        glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
        glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
        glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
      }
  }
  window = glfwCreateWindow(width, height, desc.name.c_str(), monitor, nullptr);
  if (!window) {
    spdlog::error("[App] Failed to create window");
    glfwTerminate();
    return false;
  }
  if (!graphicsContext->Initialize(window)) {
    spdlog::error("[App] Failed to initialize graphics context");
    return false;
  }
  graphicsContext->SetVSync(desc.vsync);
  SetWindowIcon();
  glfwSetWindowUserPointer(window, this);
  glfwSetCursorPosCallback(window, CursorPosCallback);
  glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
  glfwSetKeyCallback(window, KeyCallback);
  glfwSetCharCallback(window, CharCallback);
  glfwSetMouseButtonCallback(window, MouseButtonCallback);
  glfwSetScrollCallback(window, ScrollCallback);
  glfwSetWindowCloseCallback(window, WindowCloseCallback);
  if (desc.maximized && !desc.fullscreen)
    glfwMaximizeWindow(window);
  int framebufferWidth{};
  int framebufferHeight{};
  glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
  graphicsContext->Resize(framebufferWidth, framebufferHeight);
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
auto Application::SwitchScene(const std::string &name) -> bool {
  return sceneManager.Switch(name);
}
auto Application::Game(const std::string &name) -> GameBuilder {
  return GameBuilder(*this, name);
}
auto Application::SetWindowTitle(const std::string &title) -> void {
  if (window)
    glfwSetWindowTitle(window, title.c_str());
}
auto Application::AddAsset(std::unique_ptr<Asset> asset, std::string name) -> bool {
  return assetManager.Add(std::move(asset), std::move(name));
}
auto Application::SetOverlayFont(const std::filesystem::path &path, const int pixelHeight) -> bool {
  if (!overlay.SetFont(path, pixelHeight))
    return false;
  const auto id = MakeBuiltInAssetID(Overlay::AtlasAssetName);
  auto atlas = std::make_unique<TextureAsset>(id);
  atlas->texture = overlay.GetFont().GetAtlas();
  // A second call rebakes the font but cannot replace the atlas: the id is derived from the name
  // and `AssetManager::Add` leaves an id it already holds alone. Said rather than worked around,
  // because the fix is for the caller to settle on a font before the first frame -- swapping one
  // afterwards would want the texture reuploaded on both backends, which is a larger thing than a
  // font change looks.
  if (!AddAsset(std::move(atlas), Overlay::AtlasAssetName))
    spdlog::warn("[App] The overlay font was already baked; the atlas from the first call is the one that will be drawn with");
  overlay.SetAtlasAssetId(id);
  return true;
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
auto Application::EntityHasChildren(const EntityID id) const -> bool {
  if (auto scene = GetScene(); scene)
    return scene->EntityHasChildren(id);
  return false;
}
auto Application::GetAssetName(const AssetID id) const -> std::string {
  return assetManager.GetName(id);
}
auto Application::GetAssetPath(const AssetID id) const -> std::filesystem::path {
  return assetManager.GetPath(id);
}
auto Application::GetAssetType(const AssetID id) const -> AssetType {
  return assetManager.GetType(id);
}
auto Application::GetDescription() const -> const ApplicationDescription & {
  return desc;
}
auto Application::GetGraphicsContext() const -> GraphicsContext * {
  return graphicsContext.get();
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
auto Application::GetKeyAxis(const InputManager::KeyAxis axis) const -> glm::ivec2 {
  return inputManager.GetKeyAxis(axis);
}
auto Application::IsInputHeld(const int input) const -> bool {
  return inputManager.GetState(input);
}
auto Application::IsInputPressed(const int input) const -> bool {
  return inputManager.IsPressed(input);
}
auto Application::IsInputReleased(const int input) const -> bool {
  return inputManager.IsReleased(input);
}
auto Application::SetInputEnabled(const InputManager::InputKind kind, const bool enabled) -> void {
  inputManager.SetEnabled(kind, enabled);
}
auto Application::GetMousePosition() const -> glm::vec2 {
  return inputManager.GetMousePosition();
};
auto Application::GetScrollOffset() const -> glm::vec2 {
  return inputManager.GetScrollOffset();
}
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
  auto meshAsset = std::make_unique<MeshAsset>(MakeBuiltInAssetID(name));
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
    spdlog::warn("[App] Unknown primitive: {}", name);
    return;
  }
  meshAsset->bounds = BoundingBox::Calculate(meshAsset->mesh.vertices);
  if (!assetManager.Get("DefaultLit")) {
    auto defaultLit = std::make_unique<MaterialAsset>(MakeBuiltInAssetID("DefaultLit"));
    defaultLit->type = MaterialType::Lit;
    assetManager.Add(std::move(defaultLit), "DefaultLit");
  }
  if (auto defaultLit = assetManager.Get<MaterialAsset>("DefaultLit"); defaultLit)
    meshAsset->material = defaultLit->id;
  assetManager.Add(std::move(meshAsset), name);
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
auto Application::RegisterBinding(const std::string &name, const InputManager::Trigger &trigger, std::string description) -> void {
  inputManager.RegisterBinding(name, trigger, std::move(description));
}
auto Application::RegisterInputAction(const std::string &trigger, InputAction action, std::string description) -> InputManager::ActionID {
  return inputManager.RegisterAction(trigger, std::move(action), std::move(description));
}
auto Application::RegisterInputAction(int trigger, InputAction action, bool press) -> InputManager::ActionID {
  return inputManager.RegisterAction(trigger, std::move(action), press);
}
auto Application::PickEntity(const glm::vec2 &position) -> EntityID {
  auto *renderingSystem = GetRenderingSystem();
  if (!renderingSystem || !window)
    return EntityID::Invalid;
  int windowWidth{};
  int windowHeight{};
  glfwGetWindowSize(window, &windowWidth, &windowHeight);
  if (windowWidth <= 0 || windowHeight <= 0)
    return EntityID::Invalid;
  const auto [targetWidth, targetHeight] = renderingSystem->GetResolution();
  const auto x = static_cast<int>(position.x / windowWidth * targetWidth);
  const auto y = static_cast<int>(position.y / windowHeight * targetHeight);
  if (x < 0 || y < 0 || x >= targetWidth || y >= targetHeight)
    return EntityID::Invalid;
  return renderingSystem->PickEntity(x, y);
}
auto Application::PickOverlay(const glm::vec2 &position) -> int {
  auto *renderingSystem = GetRenderingSystem();
  if (!renderingSystem || !window)
    return Overlay::NoHit;
  int windowWidth{};
  int windowHeight{};
  glfwGetWindowSize(window, &windowWidth, &windowHeight);
  if (windowWidth <= 0 || windowHeight <= 0)
    return Overlay::NoHit;
  const auto [targetWidth, targetHeight] = renderingSystem->GetResolution();
  return overlay.HitTest({position.x / windowWidth * targetWidth, position.y / windowHeight * targetHeight});
}
auto Application::MarkTransformDirty(const EntityID id) -> void {
  if (auto scene = GetScene(); scene)
    scene->MarkTransformDirty(id);
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
auto Application::UnregisterInputAction(const InputManager::ActionID id) -> bool {
  return inputManager.UnregisterAction(id);
}
auto Application::GetExePath() -> std::filesystem::path {
  return GetLaunchPath();
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
      spdlog::error("[App] Failed to load icon: {}", path);
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
        spdlog::info("[App] Loaded icon: {}", path);
      } else
        spdlog::error("[App] Failed to load icon: {}", path);
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
      spdlog::info("[App] Loaded icon: {}", path);
    }
  } else {
    int width, height, channels;
    if (auto data = stbi_load(path.c_str(), &width, &height, &channels, 4); data) {
      std::array<GLFWimage, 1> images{width, height, data};
      glfwSetWindowIcon(window, 1, images.data());
      stbi_image_free(data);
      spdlog::info("[App] Loaded icon: {}", path);
    } else
      spdlog::error("[App] Failed to load icon: {}", path);
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
auto Application::FramebufferSizeCallback(GLFWwindow *window, int width, int height) -> void {
  auto instance = static_cast<Application *>(glfwGetWindowUserPointer(window));
  if (!instance || !instance->graphicsContext)
    return;
  instance->graphicsContext->Resize(width, height);
  // The render targets are not resized from here. An application that presents to its window asks
  // for the surface's size every frame in `RenderingSystem::Update`, which handles a resize as a
  // side effect and keeps one path rather than two disagreeing about who owns the resolution.
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
