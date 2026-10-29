#ifdef KUKI_HAS_DIRECTX
#include <dx_context.hpp>
#include <imgui_impl_dx12.h>
#endif
#include <GLFW/glfw3.h>
#include <algorithm>
#include <launch_path.hpp>
#include <iterator>
#include <fstream>
#include <cctype>
#include <animator.hpp>
#include <anti_aliasing.hpp>
#include <application.hpp>
#include <application_description.hpp>
#include <array>
#include <asset.hpp>
#include <asset_type.hpp>
#include <bounding_box.hpp>
#include <camera.hpp>
#include <camera_controller.hpp>
#include <camera_type.hpp>
#include <component.hpp>
#include <cstdint>
#include <editor.hpp>
#include <engine_config.hpp>
#include <enum_traits.hpp>
#include <field.hpp>
#include <filesystem>
#include <gl_render_target.hpp>
#include <gl_texture.hpp>
#include <glm/ext/quaternion_float.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <id.hpp>
#include <imgui_file_browser.hpp>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <icons_lucide.hpp>
#include <imgui_internal.h>
#include <imguizmo.hpp>
#include <key_binding_widget.hpp>
#include <light.hpp>
#include <limits>
#include <material_asset.hpp>
#include <material_type.hpp>
#include <model_asset.hpp>
#include <post_process.hpp>
#include <render_binding.hpp>
#include <profiler.hpp>
#include <property_displayer.hpp>
#include <rendering_system.hpp>
#include <scene_serializer.hpp>
#include <shader_asset.hpp>
#include <skeleton.hpp>
#include <skybox_handle.hpp>
#include <spdlog/spdlog.h>
#include <string.h>
#include <string>
#include <texture_asset.hpp>
#include <texture_content.hpp>
#include <tone_mapper.hpp>
#include <transform.hpp>
#include <utility>
#include <variant>
#include <vector>
using namespace kuki;
namespace {
#ifdef KUKI_HAS_DIRECTX
kuki::DXDescriptorHeap *gImGuiSrvHeap{};
auto ImGuiSrvAlloc(ImGui_ImplDX12_InitInfo *, D3D12_CPU_DESCRIPTOR_HANDLE *cpu, D3D12_GPU_DESCRIPTOR_HANDLE *gpu) -> void {
  if (!gImGuiSrvHeap)
    return;
  const auto index = gImGuiSrvHeap->Allocate();
  if (index == kuki::DXDescriptorHeap::InvalidIndex)
    return;
  *cpu = gImGuiSrvHeap->GetCPUHandle(index);
  *gpu = gImGuiSrvHeap->GetGPUHandle(index);
}
auto ImGuiSrvFree(ImGui_ImplDX12_InitInfo *, D3D12_CPU_DESCRIPTOR_HANDLE cpu, D3D12_GPU_DESCRIPTOR_HANDLE) -> void {
  if (!gImGuiSrvHeap)
    return;
  const auto base = gImGuiSrvHeap->GetCPUHandle(0).ptr;
  const auto size = gImGuiSrvHeap->GetDescriptorSize();
  if (size > 0 && cpu.ptr >= base)
    gImGuiSrvHeap->Free(static_cast<uint32_t>((cpu.ptr - base) / size));
}
#endif
} // namespace
namespace {
constexpr auto DEFAULT_SCENE_FILE = "scene/cornell_box.json";
struct ShortcutDef {
  const char *label;
  const char *description;
  int key;
  int mods;
};
enum ShortcutIndex : int {
  ToggleFPS,
  Copy,
  Paste,
  PlayStop,
  PauseResume,
  StepTick,
  ShortcutCount
};
constexpr ShortcutDef kShortcuts[ShortcutCount] = {
  {"Show FPS", "Toggle the FPS counter overlay in the viewport", GLFW_KEY_C, 0},
  {"Copy", "Copy the selected entities", GLFW_KEY_C, GLFW_MOD_CONTROL},
  {"Paste", "Paste the copied entities", GLFW_KEY_V, GLFW_MOD_CONTROL},
  {"Play", "Play the game in the open scene, or stop it and put the scene back", GLFW_KEY_F5, 0},
  {"Pause", "Pause the game that is playing, or resume it", GLFW_KEY_F6, 0},
  {"Step", "Run one tick of the paused game", GLFW_KEY_F10, 0},
};
struct DebugViewSequenceDef {
  const char *sequence;
  const char *targetName;
  const char *description;
};
constexpr DebugViewSequenceDef kDebugViewSequences[] = {
  {"vf", "", "Show the final render output"},
  {"vs", "ShadowMap", "Show the directional light shadow map"},
};
/// @brief A shading step and the keys that put it on the screen in place of the finished pixel.
///
/// Separate from the table above because the two select different things. That one picks which
/// render graph target is shown, which reaches every step of the lighting that ends in a resource.
/// These reach the rest: values that exist inside the scene shader for the length of an expression
/// and are summed away before anything is written, which nothing can show unless the shader is asked
/// to write them out. Most of indirect lighting is of the second kind.
struct LightingDebugSequenceDef {
  const char *sequence;
  LightingDebugView view;
  const char *description;
};
constexpr LightingDebugSequenceDef kLightingDebugSequences[] = {
  // First, and in this table rather than beside `vf`, because turning a shading view off is a
  // selection within this group and nothing to do with which graph target is being shown. It is
  // the only entry here that is not a step of the shading, and the group needs it: without a key
  // that means `None`, a view could be turned on from the keyboard and only taken off again from
  // the properties panel.
  {"vn", LightingDebugView::None, "Stop writing a shading step and go back to the finished pixel"},
  {"vi", LightingDebugView::IndirectDiffuse, "Show the probe volume's bounce on its own"},
  {"vk", LightingDebugView::SkyIrradiance, "Show the sky's diffuse contribution on its own"},
  {"vd", LightingDebugView::DirectLight, "Show direct light on its own, as a reference"},
  {"vu", LightingDebugView::SurfaceOcclusion, "Show the occlusion every indirect term is scaled by"},
  {"vy", LightingDebugView::ProbeVisibility, "Show how much of the probe field each point may believe"},
  {"vw", LightingDebugView::ProbeWeight, "Show what the eight probe corners summed to"},
  {"vb", LightingDebugView::ProbeFallback, "Show where the leak guard gave up and interpolated anyway"},
  {"vc", LightingDebugView::ProbeCell, "Show the octree leaf each point landed in"},
  {"vt", LightingDebugView::ProbeBlend, "Show where in its leaf each point sits"},
};
auto ShortcutName(int index) -> std::string {
  return std::string("Editor.") + kShortcuts[index].label;
}
struct ModelAnimationSequence final : public ImSequencer::SequenceInterface {
  const AnimationClip *clip{};
  std::vector<std::array<int, 2>> ranges;
  auto GetFrameMin() const -> int override {
    return 0;
  }
  auto GetFrameMax() const -> int override {
    return clip ? static_cast<int>(clip->duration) : 0;
  }
  auto GetItemCount() const -> int override {
    return clip ? static_cast<int>(clip->channels.size()) : 0;
  }
  auto GetItemLabel(int index) const -> const char * override {
    if (!clip || index < 0 || index >= static_cast<int>(clip->channels.size()))
      return "";
    return clip->channels[index].nodeName.c_str();
  }
  void Get(int index, int **start, int **end, int *type, unsigned int *color) override {
    if (index >= 0 && index < static_cast<int>(ranges.size())) {
      if (start)
        *start = &ranges[index][0];
      if (end)
        *end = &ranges[index][1];
    }
    if (type)
      *type = 0;
    if (color)
      *color = IM_COL32(200, 160, 60, 100);
  }
  void CustomDraw(int index, ImDrawList *drawList, const ImRect &rc, const ImRect &, const ImRect &, const ImRect &) override {
    if (!clip || index < 0 || index >= static_cast<int>(clip->channels.size()))
      return;
    const auto &channel = clip->channels[index];
    const auto duration = clip->duration > 0.f ? clip->duration : 1.f;
    const auto y = (rc.Min.y + rc.Max.y) * 0.5f;
    const auto DrawKey = [&](const float time) {
      const auto x = rc.Min.x + (time / duration) * (rc.Max.x - rc.Min.x);
      constexpr auto half = 3.5f;
      const ImVec2 points[]{{x, y - half}, {x + half, y}, {x, y + half}, {x - half, y}};
      drawList->AddConvexPolyFilled(points, 4, IM_COL32(255, 200, 0, 255));
    };
    for (const auto &key : channel.positions)
      DrawKey(key.time);
    for (const auto &key : channel.rotations)
      DrawKey(key.time);
    for (const auto &key : channel.scales)
      DrawKey(key.time);
  }
};
/// @brief A panel of the editor: one window, and one entry for it in the View menu.
///
/// `size` is a fraction of the area the panels share, which is the viewport work area minus the
/// bar. `anchor` says where in that area the window sits, 0 being the left or top edge and 1 the
/// right or bottom one, so a panel keeps its corner whatever the resolution is. A panel with a
/// `dockSlot` of its own takes that half of one shared window instead, 0 being the top half and 1
/// the bottom, and then only `size` and `anchor` of the first of them place that window. All of it
/// only decides where a panel lands the first time it is ever shown; after that its imgui.ini entry
/// wins, including a half the user has dragged out on its own.
struct WindowLayout {
  const char *name;
  ImVec2 size;
  ImVec2 anchor;
  bool open;
  int dockSlot;
  /// What stands for the panel where it is listed. Kept apart from the name, which is also the
  /// window's identity: an icon in that would change which saved layout the window answers to.
  const char *icon;
};
/// Mutable: `open` is what the View menu toggles -- the Help button, for that one -- and what a
/// window's close button clears.
WindowLayout gWindowLayouts[] = {
  {"Hierarchy", {.26f, 1.f}, {1.f, .0f}, true, 0, ICON_LC_LIST_TREE},
  {"Properties", {.26f, 1.f}, {1.f, .0f}, true, 1, ICON_LC_SLIDERS_HORIZONTAL},
  {"Settings", {.5f, .5f}, {.0f, 1.f}, false, -1, ICON_LC_SETTINGS},
  {"Assets", {.5f, .5f}, {.0f, 1.f}, false, -1, ICON_LC_FOLDER},
  {"Animation", {.5f, .5f}, {.0f, 1.f}, false, -1, ICON_LC_FILM},
  {"Profiler", {.5f, .5f}, {.0f, 1.f}, false, -1, ICON_LC_ACTIVITY},
  {"Help", {.5f, .6f}, {.5f, .5f}, false, -1, ICON_LC_CIRCLE_QUESTION_MARK},
};
/// @brief Something that ships inside the editor, and the terms it is used on.
///
/// `file` is the name `KUKI_LICENSES` in the root CMakeLists.txt gave the licence when it gathered
/// them, which is the name it is read back by. The two lists are kept in step by hand; an entry
/// here with no file there shows as a licence that could not be found, which is the right thing to
/// be told.
struct Credit {
  const char *name;
  const char *use;
  const char *licence;
};
constexpr Credit kCredits[] = {
  {"Assimp", "Model import", "BSD 3-Clause"},
  {"Dear ImGui", "The editor's interface", "MIT"},
  {"DirectX-Headers", "Direct3D 12 headers", "MIT"},
  {"GLFW", "Windows and input", "zlib"},
  {"GLM", "Vector and matrix mathematics", "The Happy Bunny Licence or MIT"},
  {"ImGuizmo", "The transform gizmo and the animation sequencer", "MIT"},
  {"imgui-filebrowser", "The file dialog", "MIT"},
  {"Inter", "The typeface", "SIL Open Font Licence 1.1"},
  {"JSON for Modern C++", "Scene and settings files", "MIT"},
  {"Kaitai Struct", "Reading icon files", "MIT"},
  {"Lucide", "The icons", "ISC"},
  {"spdlog", "Logging", "MIT"},
  {"stb", "Image loading and font rasterising", "MIT or public domain"},
  {"tinyexr", "OpenEXR images", "BSD 3-Clause"},
  {"whereami", "Finding the executable's directory", "MIT"},
};
/// @brief The rectangle a layout asks for, in screen coordinates.
auto LayoutRect(const WindowLayout &layout) -> ImRect {
  const auto *viewport = ImGui::GetMainViewport();
  const ImVec2 area(viewport->WorkSize.x, viewport->WorkSize.y);
  const ImVec2 size(layout.size.x * area.x, layout.size.y * area.y);
  const ImVec2 pos(viewport->WorkPos.x + layout.anchor.x * (area.x - size.x), viewport->WorkPos.y + layout.anchor.y * (area.y - size.y));
  return ImRect(pos, ImVec2(pos.x + size.x, pos.y + size.y));
}
/// @brief The dock node holding one half of the shared panel window, split on first use.
///
/// Dear ImGui would make a node on demand, but it would then inherit its rectangle from whichever
/// panel bound to it first, and asking for that rectangle with `SetNextWindowPos` is what undocks a
/// window in the first place. Building it outright avoids both, and splitting it in two is what
/// stacks the panels rather than tabbing them. An imgui.ini that already carries the node wins: the
/// halves are only worth resolving on a first run, when the panels have no entry to be placed by.
auto PanelDockId(const int slot) -> ImGuiID {
  static const auto rootId = ImHashStr("PanelDock");
  static ImGuiID slotIds[]{rootId, rootId};
  static auto built = false;
  // The column the two panels share runs from the top pixel of the view to the bottom one, on every
  // frame and whatever imgui.ini remembers. How wide it is and where it sits across the view are
  // the user's to change and are left as found; its height is not, because a saved layout keeps the
  // height it was saved with -- one from when a bar ran under the panels leaves a strip of that
  // bar's height under them for good, long after the bar has gone.
  if (auto *node = ImGui::DockBuilderGetNode(rootId); node) {
    const auto *viewport = ImGui::GetMainViewport();
    if (node->Pos.y != viewport->Pos.y || node->Size.y != viewport->Size.y) {
      ImGui::DockBuilderSetNodePos(rootId, ImVec2(node->Pos.x, viewport->Pos.y));
      ImGui::DockBuilderSetNodeSize(rootId, ImVec2(node->Size.x, viewport->Size.y));
    }
  }
  if (built)
    return slotIds[slot];
  built = true;
  if (ImGui::DockBuilderGetNode(rootId))
    return slotIds[slot];
  for (const auto &layout : gWindowLayouts) {
    if (layout.dockSlot < 0)
      continue;
    const auto rect = LayoutRect(layout);
    ImGui::DockBuilderAddNode(rootId);
    ImGui::DockBuilderSetNodePos(rootId, rect.Min);
    ImGui::DockBuilderSetNodeSize(rootId, rect.GetSize());
    slotIds[1] = ImGui::DockBuilderSplitNode(rootId, ImGuiDir_Down, .5f, nullptr, &slotIds[0]);
    ImGui::DockBuilderFinish(rootId);
    break;
  }
  return slotIds[slot];
}
/// @brief What a panel is coloured while the thing it controls is running, and while it is paused.
///
/// One pair for everything that can be started, paused and stopped -- the game, the profiler -- so
/// that green means running and amber means held wherever they are seen. Given the panels' own
/// alpha where they are used; only the hue is said here.
constexpr ImVec4 RUNNING_TINT(.16f, .42f, .22f, 1.f);
constexpr ImVec4 PAUSED_TINT(.5f, .38f, .1f, 1.f);
/// @brief The header of a component the running backend can do nothing with.
constexpr ImVec4 UNSUPPORTED_TINT(.72f, .36f, .08f, 1.f);
/// @brief The background a panel should have for a running/paused state, at the panels' own alpha.
auto StateTint(const bool running, const bool paused) -> ImVec4 {
  const auto &panel = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
  auto colour = !running ? panel : paused ? PAUSED_TINT : RUNNING_TINT;
  colour.w = panel.w;
  return colour;
}
/// @brief The picture that stands for a kind of component, wherever one is named.
///
/// One table for the Properties headers, the add-component menu and the Hierarchy, so that a camera
/// is the same glyph in all three. A full switch with nothing after a `default`, deliberately: a
/// component type added to the engine and not given a case here is a compiler warning, where a
/// default would quietly hand it somebody else's picture.
///
/// The backend's own objects -- buffers, programs, render targets -- share a few between them. They
/// are there to be inspected and nobody goes looking for one by its icon.
auto ComponentIcon(const ComponentType type) -> const char * {
  switch (type) {
  case ComponentType::Animator:
    return ICON_LC_FILM;
  case ComponentType::AntiAliasing:
    return ICON_LC_SCAN_LINE;
  case ComponentType::AtmosphereSky:
    return ICON_LC_SUNRISE;
  case ComponentType::BoundingBox:
    return ICON_LC_FRAME;
  case ComponentType::Camera:
    return ICON_LC_VIDEO;
  case ComponentType::MaterialHandle:
  case ComponentType::ModelMaterialHandle:
    return ICON_LC_PALETTE;
  case ComponentType::MeshHandle:
  case ComponentType::ModelMeshHandle:
    return ICON_LC_BOX;
  case ComponentType::SkyboxHandle:
    return ICON_LC_GLOBE;
  case ComponentType::TextureHandle:
    return ICON_LC_IMAGE;
  case ComponentType::IndirectLighting:
    return ICON_LC_SUN_DIM;
  case ComponentType::Light:
    return ICON_LC_LIGHTBULB;
  case ComponentType::ParticleEmitter:
    return ICON_LC_SPARKLES;
  case ComponentType::RenderBinding:
    return ICON_LC_MONITOR;
  case ComponentType::Skeleton:
    return ICON_LC_BONE;
  case ComponentType::Transform:
    return ICON_LC_MOVE_3D;
  case ComponentType::VolumetricClouds:
    return ICON_LC_CLOUDY;
  case ComponentType::VolumetricFog:
    return ICON_LC_CLOUD_FOG;
  case ComponentType::Stylization:
    return ICON_LC_BRUSH;
  case ComponentType::PlanarReflection:
    return ICON_LC_DROPLETS;
  case ComponentType::ScreenEffects:
    return ICON_LC_ZAP;
  case ComponentType::Wind:
    return ICON_LC_WIND;
  }
  return ICON_LC_CIRCLE_DASHED;
}
/// @brief A kind of entity the Hierarchy's New menu makes: a name, an icon, and the archetype.
///
/// `singleton` is the scene setting it carries, for the two checks the menu makes before offering
/// it, and `Transform` for a kind that carries none.
///
/// `asset`, where there is one, is what the kind is placed from instead of being made empty. A mesh
/// made empty had no mesh, no material and a bounding box with nothing in it, and nothing left in
/// the editor could give it any of the three: the handles have no rows in the inspector, and adding
/// components is gone. It drew as the renderer's stand-in cube and sat in the scene's bounds as
/// nothing. Placed from the Cube primitive, as one dropped from the asset browser is, it arrives
/// with all three.
struct CreatableKind {
  const char *label;
  const char *icon;
  ComponentType singleton;
  auto (*mask)() -> ComponentMask;
  const char *asset{};
};
inline constexpr CreatableKind CREATABLE[]{
  {"Empty", ICON_LC_CIRCLE_DASHED, ComponentType::Transform, &EmptyNode::Mask},
  {"Mesh", ICON_LC_BOX, ComponentType::Transform, &MeshObject::Mask, "Cube"},
  {"Camera", ICON_LC_VIDEO, ComponentType::Transform, &CameraObject::Mask},
  {"Light", ICON_LC_LIGHTBULB, ComponentType::Transform, &LightObject::Mask},
  {"Particle Emitter", ICON_LC_SPARKLES, ComponentType::ParticleEmitter, &ParticleObject::Mask},
  {"Skybox", ICON_LC_GLOBE, ComponentType::Transform, &SkyboxObject::Mask},
  {"Atmosphere Sky", ICON_LC_SUNRISE, ComponentType::AtmosphereSky, &AtmosphereSkyObject::Mask},
  {"Volumetric Clouds", ICON_LC_CLOUDY, ComponentType::VolumetricClouds, &VolumetricCloudsObject::Mask},
  {"Volumetric Fog", ICON_LC_CLOUD_FOG, ComponentType::VolumetricFog, &VolumetricFogObject::Mask},
  {"Stylization", ICON_LC_BRUSH, ComponentType::Stylization, &StylizationObject::Mask},
  {"Planar Reflection", ICON_LC_DROPLETS, ComponentType::PlanarReflection, &PlanarReflectionObject::Mask},
  {"Wind", ICON_LC_WIND, ComponentType::Wind, &WindObject::Mask},
};
/// @brief The picture that stands for an entity in the Hierarchy: that of the most telling thing it has.
///
/// Nearly everything has a transform and a bounding box, so those say nothing; what an entity is
/// for is whichever of the rarer components it carries. Asked in order, most particular first, and
/// the first one found is the answer. Something with none of them is a plain node -- an empty
/// parent, or a bone.
auto EntityIcon(const std::vector<ComponentType> &types) -> const char * {
  static constexpr ComponentType TELLING[]{ComponentType::Camera, ComponentType::Light, ComponentType::ParticleEmitter, ComponentType::Animator, ComponentType::AtmosphereSky, ComponentType::VolumetricClouds, ComponentType::VolumetricFog, ComponentType::Stylization, ComponentType::PlanarReflection, ComponentType::Wind, ComponentType::SkyboxHandle, ComponentType::IndirectLighting, ComponentType::AntiAliasing, ComponentType::ModelMeshHandle, ComponentType::MeshHandle};
  for (const auto telling : TELLING)
    if (std::ranges::find(types, telling) != types.end())
      return ComponentIcon(telling);
  return ICON_LC_CIRCLE_DASHED;
}
/// @brief A label with an icon in front, whose identity is the name alone.
///
/// The `###` is what keeps a header's open-or-closed state, and anything else kept by label, tied
/// to the name: changing which icon a component has would otherwise make every such header a
/// different widget from the one that was open a moment ago.
auto IconLabel(const char *icon, const std::string &name) -> std::string {
  return std::string(icon) + " " + name + "###" + name;
}
/// Whether the last `BeginWindow` reached `ImGui::Begin`, so `EndWindow` knows what to close.
bool gWindowBegun{};
/// @brief Begins a panel window and reports whether its contents are worth drawing.
///
/// False means the panel is toggled off on the bar, or minimized through the arrow in its title
/// bar. `EndWindow` pairs with every call either way and closes only what was really begun.
auto BeginWindow(const char *name, const ImGuiWindowFlags flags = 0) -> bool {
  WindowLayout *layout{};
  for (auto &candidate : gWindowLayouts)
    if (strcmp(candidate.name, name) == 0) {
      layout = &candidate;
      break;
    }
  gWindowBegun = false;
  if (layout && !layout->open)
    return false;
  if (layout && layout->dockSlot >= 0) {
    // No position for these: the node holds its half, and a position would undock the panel from it.
    ImGui::SetNextWindowDockID(PanelDockId(layout->dockSlot), ImGuiCond_FirstUseEver);
  } else if (layout) {
    const auto rect = LayoutRect(*layout);
    ImGui::SetNextWindowPos(rect.Min, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(rect.GetSize(), ImGuiCond_FirstUseEver);
  }
  gWindowBegun = true;
  return ImGui::Begin(name, layout ? &layout->open : nullptr, flags);
}
/// @brief Closes a panel window opened by `BeginWindow`, whatever that call returned.
auto EndWindow() -> void {
  if (gWindowBegun)
    ImGui::End();
  gWindowBegun = false;
}
} // namespace
Editor::Editor()
  : SystemApplication({.name = "Kuki Editor", .iconPath = "image/kuki.ico", .api = EngineConfig::Load().api}) {
  context.pendingApi = GetDescription().api;
  // Before anything starts, so that no tick is run on the scene merely for being open. An
  // application is playing unless told otherwise, which is right for a game and wrong for this.
  SetPlayState(PlayState::Stopped);
  for (const auto &spec : GetArchetypes())
    entityClipboard.Declare(spec);
}
auto Editor::SaveGraphicsConfig() -> void {
  auto config = EngineConfig::Load();
  config.api = context.pendingApi;
  if (auto renderingSystem = GetSystem<RenderingSystem>(); renderingSystem)
    config.toneMapper = renderingSystem->GetToneMapper();
  config.Save();
}
auto Editor::Start() -> void {
  InitImGui();
  // Read a second time rather than carried over from the constructor, which only had the backend to
  // pass down and no rendering system yet to hand the rest to.
  if (auto renderingSystem = GetSystem<RenderingSystem>(); renderingSystem) {
    const auto config = EngineConfig::Load();
    renderingSystem->SetToneMapper(config.toneMapper);
    context.capabilities = renderingSystem->GetCapabilities();
    // The editor composites the same target into a dockable panel, so a full-window blit of it
    // first would be work thrown away every frame. See `RenderingSystem::SetPresentEnabled`.
    renderingSystem->SetPresentEnabled(false);
  }
  LoadDefaultScene();
  for (auto i = 0; i < ShortcutCount; ++i)
    RegisterBinding(ShortcutName(i), InputManager::Trigger{kShortcuts[i].key, kShortcuts[i].mods}, kShortcuts[i].description);
  // Mouse look grabs the cursor and hides it from Dear ImGui, so it may only start over the game
  // view. The panels float on top of that view now, and a right click on one of them belongs to the
  // panel: its own context menus would never see the press otherwise.
  RegisterInputAction(GLFW_MOUSE_BUTTON_RIGHT, [this]() {
    if (!IsViewportHovered())
      return;
    mouselookActive = true;
    SetCursorLocked(true);
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouse;
  });
  RegisterInputAction(GLFW_MOUSE_BUTTON_RIGHT, [this]() {
    if (!mouselookActive)
      return;
    mouselookActive = false;
    SetCursorLocked(false);
    ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
  }, false);
  // The three view groups -- which graph target is shown, which shading step the scene pass writes
  // out, and what the probes are drawn as -- are independent, and each key here sets only its own.
  // Selecting within a group replaces what that group held; nothing else is touched.
  //
  // These two tables used to clear each other, on the argument that a shading view left on while
  // the final target is selected makes the target key look like it did nothing. The argument was
  // about one key rather than about the rule, and the cost of it was the rule: a view you had set
  // up came off because you looked at something else for a moment, and there was nothing to say
  // which of the keys you had pressed since was the one that took it away. Each group now keeps
  // what it was given until it is given something else, or set back to its own off entry -- `vf`
  // here, `None` in the shading list, `Off` in the probe cycle.
  for (const auto &def : kDebugViewSequences) {
    const std::string targetName = def.targetName;
    RegisterInputAction(def.sequence, [this, targetName]() {
      context.debugViewTarget = targetName;
    }, def.description);
  }
  // Not registered at all on a backend whose scene shader has no branch for them, rather than
  // registered and left to do nothing. A key that changes a label and leaves the picture alone is
  // the worst of the three options: the shortcut list still advertises it, and the person pressing
  // it concludes the view is broken rather than absent. The list is built from the same condition,
  // so what it offers and what works are one thing.
  for (const auto &def : kLightingDebugSequences) {
    if (!context.capabilities.lightingDebugViews)
      break;
    const auto view = def.view;
    RegisterInputAction(def.sequence, [this, view]() {
      if (auto *camera = GetCamera())
        camera->lightingDebugView = view;
    }, def.description);
  }
  if (context.capabilities.probeVolume) {
    RegisterInputAction("vp", [this]() {
      auto *camera = GetCamera();
      if (!camera)
        return;
      // Steps through the views rather than toggling one. There are six things a probe can be coloured
      // by and one key, and stepping is how a fault gets narrowed: the irradiance view says a probe has
      // gone wrong, and the ones after it say which of the mechanisms maintaining it is the one that
      // failed. Wrapping through `Off` keeps the way out on the same key it came in on.
      const auto &names = EnumTraits<ProbeDebugView>::GetNames();
      const auto next = (static_cast<size_t>(camera->probeDebugView) + 1) % names.size();
      camera->probeDebugView = static_cast<ProbeDebugView>(next);
    }, "Step through the irradiance probe visualisations");
  }
}
auto Editor::Update(const float deltaTime) -> void {
  KUKI_PROFILE_SCOPE("Editor");
  // On the first frame rather than in `Start`, so that the scene's first frame is drawn stopped and
  // the camera controller has its keys before play begins a session that would take them.
  if (startPlaying) {
    startPlaying = false;
    BeginPlay();
  }
  UpdateIO();
  UpdateView();
}
auto Editor::Shutdown() -> void {
#ifdef KUKI_HAS_DIRECTX
  if (auto *dx = dynamic_cast<DXContext *>(GetGraphicsContext()); dx) {
    dx->WaitForGPU();
    ImGui_ImplDX12_Shutdown();
    gImGuiSrvHeap = nullptr;
  } else
#endif
    ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
}
auto Editor::SetInputCaptureActive(bool active) -> void {
  context.state = active ? EditorState::RebindingKey : EditorState::Normal;
}
auto Editor::GetFPS() -> size_t {
  if (auto renderingSystem = GetRenderingSystem(); renderingSystem)
    return renderingSystem->GetFPS();
  return 0;
}
auto Editor::GetPreviewSize() -> int {
  if (auto renderingSystem = GetRenderingSystem(); renderingSystem)
    return renderingSystem->GetPreviewSize();
  return 0;
}
auto Editor::PreviewAsset(const AssetID id) -> RenderTarget * {
  if (auto renderingSystem = GetRenderingSystem(); renderingSystem)
    return renderingSystem->PreviewAsset(id);
  return nullptr;
}
auto Editor::SetPreviewSize(const int size) -> void {
  if (auto renderingSystem = GetRenderingSystem(); renderingSystem)
    renderingSystem->SetPreviewSize(size);
}
auto Editor::SetResolution(const int width, const int height) -> void {
  if (auto renderingSystem = GetRenderingSystem(); renderingSystem)
    renderingSystem->SetResolution(width, height);
}
auto Editor::HasComponentAnywhere(const ComponentType type) const -> bool {
  if (auto scene = GetScene(); scene)
    return scene->HasComponentAnywhere(type);
  return false;
}
auto Editor::BeginKeyCapture() -> void {
  GetInputManager().BeginKeyCapture();
}
auto Editor::PollKeyCapture() -> InputManager::CaptureOutcome {
  return GetInputManager().PollKeyCapture();
}
auto Editor::GetBinding(const std::string &name) -> InputManager::Trigger {
  return GetInputManager().GetBinding(name);
}
auto Editor::SetBinding(const std::string &name, const InputManager::Trigger &trigger) -> void {
  GetInputManager().SetBinding(name, trigger);
}
auto Editor::GetTriggerName(const InputManager::Trigger &trigger) -> std::string {
  return GetInputManager().GetTriggerName(trigger);
}
auto Editor::GetBindingNames() -> const std::vector<std::string> & {
  return GetInputManager().GetBindingNames();
}
auto Editor::GetBindingDescription(const std::string &name) -> std::string {
  return GetInputManager().GetBindingDescription(name);
}
auto Editor::GetSequenceNames() -> const std::vector<std::string> & {
  return GetInputManager().GetSequenceNames();
}
auto Editor::GetSequenceDescription(const std::string &sequence) -> std::string {
  return GetInputManager().GetSequenceDescription(sequence);
}
auto Editor::GetSelectedEntity() const -> EntityID {
  return context.selectedEntityId;
}
auto Editor::GetSelectedEntities() const -> std::vector<EntityID> {
  return {context.selectedEntities.begin(), context.selectedEntities.end()};
}
auto Editor::InitImGui() -> void {
  auto constexpr FONT_SIZE = 16.f;
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  auto &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad | ImGuiConfigFlags_DockingEnable;
  // The panels float over a full screen game view, so dragging must start on the title bar: without
  // this a drag that begins on empty panel space would move the window instead of reaching the tool.
  io.ConfigWindowsMoveFromTitleBarOnly = true;
  auto fontPath = std::filesystem::path{GetDescription().path / "font/Inter-VariableFont_opsz,wght.ttf"}.string();
  // The text font is told to leave the icons' codepoints alone. They are in the Private Use Area,
  // which is nobody's, and Inter draws things of its own there -- circled digits, ticks in circles
  // -- and in a merged font the first font to have a glyph is the one that draws it. Left as it
  // was, Play was a tick in a circle and Pause a circled four: the right codepoint, out of the
  // wrong font.
  static constexpr ImWchar ICON_RANGE[]{ICON_LC_MIN, ICON_LC_MAX, 0};
  ImFontConfig text;
  text.GlyphExcludeRanges = ICON_RANGE;
  io.Fonts->AddFontFromFileTTF(fontPath.c_str(), FONT_SIZE, &text);
  // The icons, merged into the font above so that one can sit in a label beside its text: after
  // this an icon is a character like any other, written with the names in `icons_lucide.hpp`.
  // Given the width of the text's own line height each, so that a column of labels that begin with
  // different icons still begins its words at one place.
  ImFontConfig icons;
  icons.MergeMode = true;
  icons.GlyphMinAdvanceX = FONT_SIZE;
  // Lucide draws in a square that sits on the baseline, which puts an icon visibly higher than the
  // middle of the lower-case letters beside it. Lowered by a tenth of the size to sit level.
  icons.GlyphOffset.y = FONT_SIZE * .1f;
  const auto iconPath = std::filesystem::path{GetDescription().path / "font/lucide.ttf"}.string();
  if (!io.Fonts->AddFontFromFileTTF(iconPath.c_str(), FONT_SIZE, &icons))
    spdlog::warn("[Editor] The icon font did not load from {}, so icons will show as missing glyphs", iconPath);
  auto &style = ImGui::GetStyle();
  style.ChildRounding = .0f;
  style.FrameRounding = .0f;
  style.TabRounding = .0f;
  style.WindowRounding = .0f;
  // The panels sit on top of the game view rather than beside it, so let it show through them.
  auto constexpr WINDOW_ALPHA = .8f;
  auto constexpr TITLE_ALPHA = .9f;
  style.Colors[ImGuiCol_WindowBg].w = WINDOW_ALPHA;
  style.Colors[ImGuiCol_TitleBg].w = TITLE_ALPHA;
  style.Colors[ImGuiCol_TitleBgActive].w = TITLE_ALPHA;
  style.Colors[ImGuiCol_TitleBgCollapsed].w = TITLE_ALPHA;
#ifdef KUKI_HAS_DIRECTX
  if (auto *dx = dynamic_cast<DXContext *>(GetGraphicsContext()); dx) {
    ImGui_ImplGlfw_InitForOther(window, true);
    gImGuiSrvHeap = &dx->GetSRVHeap();
    ImGui_ImplDX12_InitInfo info{};
    info.Device = dx->GetDevice();
    info.CommandQueue = dx->GetCommandQueue();
    info.NumFramesInFlight = static_cast<int>(DX_FRAME_COUNT);
    info.RTVFormat = dx->GetBackBufferFormat();
    info.DSVFormat = DXGI_FORMAT_UNKNOWN;
    info.SrvDescriptorHeap = dx->GetSRVHeap().Get();
    info.SrvDescriptorAllocFn = ImGuiSrvAlloc;
    info.SrvDescriptorFreeFn = ImGuiSrvFree;
    ImGui_ImplDX12_Init(&info);
  } else
#endif
  {
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init();
  }
  ImGuizmo::SetImGuiContext(ImGui::GetCurrentContext());
  fileBrowser.SetTitle("Browse Files");
  sceneFileBrowser.SetTypeFilters({".json"});
}
auto Editor::LoadDefaultScene() -> void {
  const auto sceneName = "Main";
  CreateScene(sceneName);
  const auto scenePath = GetDescription().path / DEFAULT_SCENE_FILE;
  if (!SceneSerializer::Load(*this, scenePath)) {
    spdlog::error("[Editor] Failed to load default scene: {}", scenePath.string());
    return;
  }
  AttachCameraController();
  AttachSettingsEntity();
}
auto Editor::UpdateIO() -> void {
  static auto stateOld = EditorState::Normal;
  if (context.state != stateOld) {
    stateOld = context.state;
    const auto typing = context.state == EditorState::Rename || context.state == EditorState::RebindingKey;
    SetInputEnabled(InputManager::InputKind::Keys, !typing);
    if (context.state != EditorState::Normal) {
      context.keyState.reset();
      context.pressState.reset();
      context.releaseState.reset();
      return;
    }
  }
  const auto &io = ImGui::GetIO();
  context.keyState.set(static_cast<uint8_t>(KeyBit::Alt), io.KeyAlt);
  context.keyState.set(static_cast<uint8_t>(KeyBit::Ctrl), io.KeyCtrl);
  context.keyState.set(static_cast<uint8_t>(KeyBit::Shift), io.KeyShift);
  context.pressState.set(static_cast<uint8_t>(KeyBit::Backspace), ImGui::IsKeyPressed(ImGuiKey_Backspace));
  context.pressState.set(static_cast<uint8_t>(KeyBit::Delete), ImGui::IsKeyPressed(ImGuiKey_Delete));
  context.pressState.set(static_cast<uint8_t>(KeyBit::Enter), ImGui::IsKeyPressed(ImGuiKey_Enter));
  context.pressState.set(static_cast<uint8_t>(KeyBit::Escape), ImGui::IsKeyPressed(ImGuiKey_Escape));
  context.pressState.set(static_cast<uint8_t>(KeyBit::Space), ImGui::IsKeyPressed(ImGuiKey_Space));
  context.releaseState.set(static_cast<uint8_t>(KeyBit::Backspace), ImGui::IsKeyReleased(ImGuiKey_Backspace));
  context.releaseState.set(static_cast<uint8_t>(KeyBit::Delete), ImGui::IsKeyReleased(ImGuiKey_Delete));
  context.releaseState.set(static_cast<uint8_t>(KeyBit::Enter), ImGui::IsKeyReleased(ImGuiKey_Enter));
  context.releaseState.set(static_cast<uint8_t>(KeyBit::Escape), ImGui::IsKeyReleased(ImGuiKey_Escape));
  context.releaseState.set(static_cast<uint8_t>(KeyBit::Space), ImGui::IsKeyReleased(ImGuiKey_Space));
}
auto Editor::UpdateView() -> void {
  const auto wasPicking = context.state == EditorState::PickingAsset;
#ifdef KUKI_HAS_DIRECTX
  const auto usingDX = dynamic_cast<DXContext *>(GetGraphicsContext()) != nullptr;
  if (usingDX)
    ImGui_ImplDX12_NewFrame();
  else
#endif
    ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  ImGuizmo::BeginFrame();
  DisplayMenuBar();
  DisplayScene();
  DisplayAssets();
  DisplayAnimation();
  DisplayProfiler();
  DisplayHierarchy();
  DisplayProperties();
  DisplaySettings();
  DisplayHelp();
  // After the frame's panels rather than where the request was made, which was inside a system: the
  // scene it is in is about to be replaced.
  if (TakeStopRequest())
    EndPlay();
  if (wasPicking && context.state == EditorState::PickingAsset && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    context.state = EditorState::Normal;
  ImGui::Render();
#ifdef KUKI_HAS_DIRECTX
  if (usingDX) {
    auto *dx = static_cast<DXContext *>(GetGraphicsContext());
    if (auto *commandList = dx->GetCommandList(); commandList) {
      const auto rtv = dx->GetBackBufferRTV();
      commandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
      auto *heap = dx->GetSRVHeap().Get();
      commandList->SetDescriptorHeaps(1, &heap);
      ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
    }
  } else
#endif
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
auto Editor::ApplyPickedAsset(const AssetID assetId) -> void {
  switch (context.pickingTarget) {
  case PickingTarget::MaterialTexture: {
    auto *binding = GetEntityComponent<RenderBinding>(context.pickingEntityId);
    if (auto *material = binding && binding->glMaterialBound ? &binding->glMaterial : nullptr; material) {
      const auto texture = PreviewAsset(assetId);
      if (texture) {
        const auto handle = static_cast<unsigned int>(texture->GetTextureHandle());
        switch (context.pickingProperty) {
        case MaterialProperty::AlbedoTexture:
          material->textures.albedo = handle;
          break;
        case MaterialProperty::NormalTexture:
          material->textures.normal = handle;
          break;
        case MaterialProperty::MetalnessTexture:
          material->textures.metalness = handle;
          break;
        case MaterialProperty::OcclusionTexture:
          material->textures.occlusion = handle;
          break;
        case MaterialProperty::RoughnessTexture:
          material->textures.roughness = handle;
          break;
        case MaterialProperty::SpecularTexture:
          material->textures.specular = handle;
          break;
        case MaterialProperty::EmissiveTexture:
          material->textures.emissive = handle;
          break;
        }
      }
    }
    break;
  }
  case PickingTarget::Skybox: {
    // Into the handle the entity has. It used to be given one if it had none, and an entity's
    // components are its archetype's now; an entity with no sky to set is not offered the picker.
    auto *handle = GetEntityComponent<SkyboxHandle>(context.pickingEntityId);
    if (handle && handle->assetId != assetId) {
      handle->assetId = assetId;
      handle->resourceId = EntityID::Invalid;
    }
    break;
  }
  }
  context.state = EditorState::Normal;
  context.pickingEntityId = EntityID::Invalid;
}
auto Editor::DisplayAssets() -> void {
  const auto previewSize = static_cast<float>(GetPreviewSize());
  if (BeginWindow("Assets"))
    DisplayAssetBrowser(previewSize);
  EndWindow();
}
auto Editor::DisplayAssetBrowser(const float previewSize) -> void {
  static constexpr ImVec2 FLIP_UV0(0.f, 1.f);
  static constexpr ImVec2 FLIP_UV1(1.f, 0.f);
  static constexpr ImVec2 UV0(0.f, 0.f);
  static constexpr ImVec2 UV1(1.f, 1.f);
  static constexpr auto POPUP_WINDOW_FLAGS = ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight;
  static constexpr auto LIST_ITEM_WIDTH = 160.f;
  const auto escapePressed = context.pressState.test(static_cast<uint8_t>(KeyBit::Escape));
  const auto picking = context.state == EditorState::PickingAsset;
  if (picking) {
    const auto min = ImGui::GetWindowPos();
    const auto max = ImVec2(min.x + ImGui::GetWindowSize().x, min.y + ImGui::GetWindowSize().y);
    ImGui::GetWindowDrawList()->AddRect(min, max, PICKING_HIGHLIGHT_COLOR, 0.f, 0, PICKING_HIGHLIGHT_THICKNESS);
    if (escapePressed)
      context.state = EditorState::Normal;
  }
  const auto pickingSkybox = picking && context.pickingTarget == PickingTarget::Skybox;
  std::vector<const Asset *> assets;
  auto CollectAsset = [&](const Asset *asset) {
    if (pickingSkybox) {
      const auto textureAsset = asset->As<TextureAsset>();
      if (!textureAsset || textureAsset->texture.content != TextureContent::Skybox)
        return;
    }
    assets.push_back(asset);
  };
  if (picking)
    ForEachAsset(context.pickingAssetType, CollectAsset);
  else
    ForEachAssetType([this, &CollectAsset](const AssetType type, const std::string &) { ForEachAsset(type, CollectAsset); });
  const auto DisplayContextMenu = [this] {
    if (ImGui::BeginPopupContextWindow("AssetBrowserMenu", POPUP_WINDOW_FLAGS)) {
      if (ImGui::MenuItem("Import")) {
        fileBrowser.Open();
        ImGui::CloseCurrentPopup();
      }
      ImGui::Separator();
      if (ImGui::MenuItem("List View", nullptr, context.assetViewMode == AssetViewMode::List))
        context.assetViewMode = AssetViewMode::List;
      if (ImGui::MenuItem("Thumbnail View", nullptr, context.assetViewMode == AssetViewMode::Thumbnail))
        context.assetViewMode = AssetViewMode::Thumbnail;
      ImGui::EndPopup();
    }
  };
  const auto &style = ImGui::GetStyle();
  if (context.assetViewMode == AssetViewMode::List) {
    const auto rowHeight = ImGui::GetTextLineHeightWithSpacing();
    const auto rowsPerColumn = (std::max)(1, static_cast<int>(ImGui::GetContentRegionAvail().y / rowHeight));
    const auto selectableWidth = LIST_ITEM_WIDTH - style.ItemSpacing.x;
    ImGui::BeginChild("##AssetList", ImVec2(0.f, 0.f), false, ImGuiWindowFlags_HorizontalScrollbar);
    for (size_t i = 0; i < assets.size(); ++i) {
      const auto id = assets[i]->id;
      const auto column = static_cast<int>(i) / rowsPerColumn;
      const auto row = static_cast<int>(i) % rowsPerColumn;
      ImGui::SetCursorPos(ImVec2(column * LIST_ITEM_WIDTH, row * rowHeight));
      const auto name = GetAssetName(id);
      ImGui::PushID(static_cast<int>(id));
      const auto selected = ImGui::Selectable(name.c_str(), false, ImGuiSelectableFlags_None, ImVec2(selectableWidth, 0.f));
      if (picking && selected)
        ApplyPickedAsset(id);
      if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        ImGui::SetDragDropPayload("##SpawnPayload", &id, sizeof(AssetID));
        ImGui::EndDragDropSource();
      }
      if (ImGui::IsItemHovered()) {
        const auto texture = PreviewAsset(id);
        if (texture && ImGui::BeginTooltip()) {
          const auto flip = texture->NeedsVerticalFlip();
          const auto &uv0 = flip ? FLIP_UV0 : UV0;
          const auto &uv1 = flip ? FLIP_UV1 : UV1;
          TextureButton("##ListPreview", static_cast<ImTextureID>(texture->GetTextureHandle()), ImVec2(previewSize, previewSize), uv0, uv1);
          ImGui::EndTooltip();
        }
      }
      ImGui::PopID();
    }
    DisplayContextMenu();
    ImGui::EndChild();
  } else {
    const auto cellWidth = previewSize + style.FramePadding.x * 2.f + style.ItemSpacing.x;
    const auto columnCount = static_cast<size_t>((std::max)(1, static_cast<int>(ImGui::GetContentRegionAvail().x / cellWidth)));
    ImGui::BeginChild("##AssetThumbnails", ImVec2(0.f, 0.f), false);
    for (size_t i = 0; i < assets.size(); ++i) {
      const auto id = assets[i]->id;
      const auto name = GetAssetName(id);
      if (i % columnCount != 0)
        ImGui::SameLine();
      ImGui::PushID(static_cast<int>(id));
      ImGui::BeginGroup();
      const auto texture = PreviewAsset(id);
      const auto tex = texture ? static_cast<ImTextureID>(texture->GetTextureHandle()) : ImTextureID{};
      const auto flip = !texture || texture->NeedsVerticalFlip();
      const auto &uv0 = flip ? FLIP_UV0 : UV0;
      const auto &uv1 = flip ? FLIP_UV1 : UV1;
      const auto clicked = TextureButton("##Thumbnail", tex, ImVec2(previewSize, previewSize), uv0, uv1);
      if (picking && clicked)
        ApplyPickedAsset(id);
      if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        ImGui::SetDragDropPayload("##SpawnPayload", &id, sizeof(AssetID));
        ImGui::EndDragDropSource();
      }
      ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + previewSize);
      ImGui::TextWrapped("%s", name.c_str());
      ImGui::PopTextWrapPos();
      ImGui::EndGroup();
      ImGui::PopID();
    }
    DisplayContextMenu();
    ImGui::EndChild();
  }
  fileBrowser.Display();
  if (fileBrowser.HasSelected()) {
    const auto filepath = fileBrowser.GetSelected();
    if (!std::filesystem::exists(filepath))
      spdlog::error("[Editor] File does not exist: {}", filepath.string());
    else {
      const auto ext = filepath.extension();
      if (ext == ".vert" || ext == ".frag" || ext == ".comp")
        LoadAssetAsync<ShaderAsset>(filepath);
      else if (ext == ".gltf" || ext == ".glb" || ext == ".fbx")
        LoadAssetAsync<ModelAsset>(filepath);
      else if (ext == ".hdr" || ext == ".exr")
        LoadAssetAsync<TextureAsset>(filepath);
      else if (ext == ".mat")
        LoadAssetAsync<MaterialAsset>(filepath);
    }
    fileBrowser.ClearSelected();
  }
}
auto Editor::DisplayAnimation() -> void {
  if (!BeginWindow("Animation")) {
    EndWindow();
    return;
  }
  Animator *animator{};
  Skeleton *skeleton{};
  if (!context.selectedEntities.empty()) {
    auto id = *context.selectedEntities.begin();
    for (auto i = 0; i < 64 && id && !animator; ++i) {
      animator = GetEntityComponent<Animator>(id);
      skeleton = GetEntityComponent<Skeleton>(id);
      if (!animator)
        id = GetEntityParent(id);
    }
  }
  if (!animator || !skeleton) {
    ImGui::TextDisabled("Select an entity with an Animator component to preview its animation.");
    EndWindow();
    return;
  }
  auto modelAsset = GetAsset<ModelAsset>(animator->modelAssetId);
  if (!modelAsset || animator->clipIndex < 0 || animator->clipIndex >= static_cast<int>(modelAsset->animations.size())) {
    ImGui::TextDisabled("Selected entity's Animator has no valid clip assigned.");
    EndWindow();
    return;
  }
  auto &clip = modelAsset->animations[animator->clipIndex];
  field::Toggle(ICON_LC_PLAY, "Playing", &animator->playing);
  ImGui::SameLine();
  field::Toggle(ICON_LC_REPEAT, "Loop", &animator->loop);
  ImGui::SameLine();
  const auto seconds = clip.ticksPerSecond > 0.f ? clip.duration / clip.ticksPerSecond : 0.f;
  ImGui::TextDisabled("%s (%.2fs)", clip.name.c_str(), seconds);
  ModelAnimationSequence sequence;
  sequence.clip = &clip;
  sequence.ranges.assign(clip.channels.size(), {0, static_cast<int>(clip.duration)});
  auto currentFrame = static_cast<int>(animator->time);
  static bool expanded = true;
  auto selectedEntry = -1;
  auto firstFrame = 0;
  if (ImSequencer::Sequencer(&sequence, &currentFrame, &expanded, &selectedEntry, &firstFrame, ImSequencer::SEQUENCER_CHANGE_FRAME))
    animator->time = static_cast<float>(currentFrame);
  EndWindow();
}
auto Editor::DisplayEntity(const EntityID id) -> void {
  static constexpr auto TREE_NODE_FLAGS = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_NavLeftJumpsToParent;
  static constexpr auto INPUT_TEXT_FLAGS = ImGuiTreeNodeFlags(ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
  static constexpr auto NAME_LENGTH = 256;
  static char newName[NAME_LENGTH] = "";
  static auto renameBufferOwner = EntityID::Invalid;
  auto nodeFlags = ImGuiTreeNodeFlags(TREE_NODE_FLAGS);
  if (!EntityHasChildren(id))
    nodeFlags |= ImGuiTreeNodeFlags_Leaf;
  if (context.selectedEntities.contains(id))
    nodeFlags |= ImGuiTreeNodeFlags_Selected;
  if (context.state == EditorState::Rename && context.renamedEntityId == id) {
    if (renameBufferOwner != id) {
      const auto currentName = GetEntityName(id);
      strncpy(newName, currentName.c_str(), NAME_LENGTH - 1);
      newName[NAME_LENGTH - 1] = '\0';
      renameBufferOwner = id;
    }
    ImGui::AlignTextToFramePadding();
    ImGui::PushItemWidth(-1);
    ImGui::SetKeyboardFocusHere();
    auto renamed = ImGui::InputText("##Rename", newName, NAME_LENGTH, INPUT_TEXT_FLAGS);
    if (renamed) {
      std::string nameStr = newName;
      if (!nameStr.empty())
        RenameEntity(id, nameStr);
    }
    if (renamed || ImGui::IsItemDeactivated()) {
      context.state = EditorState::Normal;
      context.renamedEntityId = EntityID::Invalid;
      renameBufferOwner = EntityID::Invalid;
    }
    ImGui::PopItemWidth();
    return;
  }
  const auto entityName = GetEntityName(id);
  const auto entityNameCStr = entityName.c_str();
  const auto nodeOpen = ImGui::TreeNodeEx(static_cast<const void *>(id), nodeFlags, "%s %s", EntityIcon(GetEntityComponentTypes(id)), entityNameCStr);
  displayedEntitiesNext.push_back(id);
  const auto doubleClicked = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
  const auto focused = ImGui::IsItemFocused();
  const auto hovered = ImGui::IsItemHovered();
  const auto ctrlHeld = context.keyState.test(static_cast<uint8_t>(KeyBit::Ctrl));
  const auto enterPressed = context.pressState.test(static_cast<uint8_t>(KeyBit::Enter));
  const auto shiftHeld = context.keyState.test(static_cast<uint8_t>(KeyBit::Shift));
  const auto spacePressed = context.pressState.test(static_cast<uint8_t>(KeyBit::Space));
  const auto navPressed = ImGui::IsKeyPressed(ImGuiKey_UpArrow) || ImGui::IsKeyPressed(ImGuiKey_DownArrow) || ImGui::IsKeyPressed(ImGuiKey_LeftArrow) || ImGui::IsKeyPressed(ImGuiKey_RightArrow);
  const auto clicked = (shiftHeld || ctrlHeld) ? ImGui::IsItemClicked(ImGuiMouseButton_Left) : (ImGui::IsItemDeactivated() && hovered);
  if (context.state == EditorState::Normal) {
    if (clicked) {
      if (shiftHeld) {
        auto it1 = std::find(displayedEntities.begin(), displayedEntities.end(), context.selectedEntityId);
        auto it2 = std::find(displayedEntities.begin(), displayedEntities.end(), id);
        if (it1 != displayedEntities.end() && it2 != displayedEntities.end()) {
          if (it1 > it2)
            std::swap(it1, it2);
          if (!ctrlHeld)
            context.selectedEntities.clear();
          for (auto it = it1; it <= it2; ++it)
            context.selectedEntities.insert(*it);
        }
      } else if (ctrlHeld) {
        if (context.selectedEntities.contains(id)) {
          context.selectedEntities.erase(id);
          context.selectedEntityId = EntityID::Invalid;
        } else
          context.selectedEntities.insert(id);
      } else {
        context.selectedEntities.clear();
        context.selectedEntities.insert(id);
      }
      context.selectedEntityId = id;
    } else if (focused && navPressed && (shiftHeld || ctrlHeld)) {
      context.selectedEntities.insert(id);
      context.selectedEntityId = id;
    } else if (focused && (enterPressed || spacePressed)) {
      context.selectedEntities.clear();
      context.selectedEntityId = id;
    }
  }
  if (hovered && doubleClicked) {
    context.state = EditorState::Rename;
    context.renamedEntityId = id;
  }
  if (context.state == EditorState::Normal && ImGui::IsMouseReleased(ImGuiMouseButton_Right) && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup)) {
    if (!context.selectedEntities.contains(id)) {
      context.selectedEntities.clear();
      context.selectedEntities.insert(id);
      context.selectedEntityId = id;
    }
    ImGui::OpenPopup("##EntityContextMenu");
    entityContextMenuTriggered = true;
  }
  // Dragged, the row is the entity, to be dropped on a field that takes one: an inverse kinematics
  // goal's target. After everything above that asks about the row, since a drag's tooltip is a
  // window of its own and the row would no longer be the last thing drawn.
  if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
    ImGui::SetDragDropPayload("##EntityPayload", &id, sizeof(EntityID));
    ImGui::Text("%s %s", EntityIcon(GetEntityComponentTypes(id)), entityNameCStr);
    ImGui::EndDragDropSource();
  }
  if (ImGui::BeginPopup("##EntityContextMenu")) {
    if (ImGui::MenuItem("Copy"))
      CopySelectedEntities();
    if (ImGui::MenuItem("Paste", nullptr, false, !entityClipboardRoots.empty()))
      PasteEntities();
    ImGui::Separator();
    if (ImGui::MenuItem("Delete"))
      for (const auto &entityId : context.selectedEntities)
        DeleteEntity(entityId);
    ImGui::EndPopup();
  }
  if (nodeOpen) {
    ForEachChildEntity(id, [&](const EntityID childId) {
      ImGui::PushID(static_cast<int>(childId));
      DisplayEntity(childId);
      ImGui::PopID();
    });
    ImGui::TreePop();
  }
}
auto Editor::DisplayHierarchy() -> void {
  static constexpr auto POPUP_WINDOW_FLAGS = ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight;
  if (!BeginWindow("Hierarchy")) {
    EndWindow();
    return;
  }
  const auto clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
  const auto windowHovered = ImGui::IsWindowHovered();
  const auto itemsHovered = ImGui::IsAnyItemHovered();
  const auto backspacePressed = context.pressState.test(static_cast<uint8_t>(KeyBit::Backspace));
  const auto deletePressed = context.pressState.test(static_cast<uint8_t>(KeyBit::Delete));
  const auto escapePressed = context.pressState.test(static_cast<uint8_t>(KeyBit::Escape));
  const auto clearSelection = (windowHovered && !itemsHovered) && (clicked || backspacePressed || deletePressed || escapePressed);
  if (context.state == EditorState::Normal) {
    if (deletePressed)
      for (const auto &entityId : context.selectedEntities)
        DeleteEntity(entityId);
    if (IsBindingPressed(ShortcutName(Copy)))
      CopySelectedEntities();
    if (IsBindingPressed(ShortcutName(Paste)))
      PasteEntities();
    if (clearSelection) {
      context.selectedEntities.clear();
      context.selectedEntityId = EntityID::Invalid;
    }
  }
  displayedEntitiesNext.clear();
  entityContextMenuTriggered = false;
  ForEachRootEntity([this](const EntityID id) {
    ImGui::PushID(static_cast<int>(id));
    DisplayEntity(id);
    ImGui::PopID();
  });
  displayedEntities = std::move(displayedEntitiesNext);
  if (!entityContextMenuTriggered && ImGui::BeginPopupContextWindow("CreateMenu", POPUP_WINDOW_FLAGS)) {
    if (ImGui::BeginMenu("New")) {
      // What an entity is made of is chosen here and kept: nothing adds a component to it later.
      // So the choice is of a kind of entity, not of an empty one to fill in.
      for (const auto &kind : CREATABLE) {
        // A scene setting the scene already has is not offered again, and nor is one the running
        // backend cannot act on -- see the same rule in the property panels.
        const auto taken = kind.singleton != ComponentType::Transform && HasSceneSingleton(kind.singleton);
        const auto supported = kind.singleton == ComponentType::Transform || IsSupported(kind.singleton);
        if (taken || !supported)
          continue;
        if (ImGui::MenuItem(IconLabel(kind.icon, kind.label).c_str())) {
          auto id = EntityID::Invalid;
          if (kind.asset) {
            id = InstantiateAsset(kind.asset);
            if (id)
              RenameEntity(id, kind.label);
          } else
            id = CreateEntity(kind.mask(), kind.label);
          context.state = EditorState::Rename;
          context.renamedEntityId = id;
          spdlog::info("[Editor] Created a new {}", kind.label);
          ImGui::CloseCurrentPopup();
        }
      }
      ImGui::EndMenu();
    }
    if (ImGui::MenuItem("Paste", nullptr, false, !entityClipboardRoots.empty())) {
      PasteEntities();
      ImGui::CloseCurrentPopup();
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Save Scene...")) {
      sceneFileAction = SceneFileAction::Save;
      sceneFileBrowser.SetTitle("Save Scene");
      sceneFileBrowser.Open();
      ImGui::CloseCurrentPopup();
    }
    if (ImGui::MenuItem("Load Scene...")) {
      sceneFileAction = SceneFileAction::Load;
      sceneFileBrowser.SetTitle("Load Scene");
      sceneFileBrowser.Open();
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
  sceneFileBrowser.Display();
  if (sceneFileBrowser.HasSelected()) {
    auto path = sceneFileBrowser.GetSelected();
    if (path.extension() != ".json")
      path += ".json";
    if (sceneFileAction == SceneFileAction::Save)
      SaveScene(path);
    else
      LoadScene(path);
    sceneFileBrowser.ClearSelected();
  }
  EndWindow();
}
auto Editor::SaveScene(const std::filesystem::path &path) -> void {
  if (SceneSerializer::Save(*this, path))
    spdlog::info("[Editor] Saved scene to: {}", path.string());
  else
    spdlog::error("[Editor] Failed to save scene to: {}", path.string());
}
auto Editor::LoadScene(const std::filesystem::path &path) -> void {
  if (!SceneSerializer::Load(*this, path)) {
    spdlog::error("[Editor] Failed to load scene from: {}", path.string());
    return;
  }
  context.selectedEntities.clear();
  context.selectedEntityId = EntityID::Invalid;
  context.state = EditorState::Normal;
  displayedEntities.clear();
  AttachCameraController();
  AttachSettingsEntity();
  spdlog::info("[Editor] Loaded scene from: {}", path.string());
}
auto Editor::HasSceneSingleton(const ComponentType type) const -> bool {
  // Anywhere in the scene, not merely on the selected entity -- that is what makes it a property of
  // the scene rather than of whatever carries it. The caller has already established this entity
  // does not have one, so finding one at all means it is somewhere else.
  return Component::IsSceneSingleton(type) && HasComponentAnywhere(type);
}
auto Editor::AttachSettingsEntity() -> EntityID {
  // Found by the component rather than by the entity's name, which is the same way the camera
  // controller finds its camera. A name would be a second thing to keep in step, and the component
  // is what actually matters here -- whatever it is sitting on is the settings entity.
  //
  // Reused rather than replaced when one is already there, so reloading a scene does not throw away
  // tuning that was in the middle of being done.
  //
  // Made as an `EditorSettings` when there is none, which has every setting on it; one found already
  // is whatever it was made as, and keeps it.
  auto id = EntityID::Invalid;
  ForEachEntity<IndirectLighting>([&id](const EntityID entity, IndirectLighting *) {
    if (!id)
      id = entity;
  });
  if (!id)
    id = EditorWorld::Create<EditorSettings>(*this, "Settings");
  return id;
}
auto Editor::AttachCameraController() -> EntityID {
  auto cameraId = EntityID::Invalid;
  ForEachEntity<Camera>([&cameraId](const EntityID id, Camera *) {
    if (!cameraId)
      cameraId = id;
  });
  if (!cameraId)
    return cameraId;
  context.cameraEntity = cameraId;
  SetActiveCamera(cameraId);
  if (auto *controller = GetSystem<CameraController>(); controller)
    controller->SetEntity(cameraId);
  return cameraId;
}
auto Editor::CopySelectedEntities() -> void {
  auto scene = GetScene();
  if (!scene || context.selectedEntities.empty())
    return;
  entityClipboard.Clear();
  entityClipboardRoots.clear();
  for (const auto &id : context.selectedEntities) {
    auto ancestorSelected = false;
    for (auto parent = GetEntityParent(id); parent; parent = GetEntityParent(parent))
      if (context.selectedEntities.contains(parent)) {
        ancestorSelected = true;
        break;
      }
    if (ancestorSelected)
      continue;
    if (const auto clipboardId = scene->CopyEntityTo(id, entityClipboard); clipboardId)
      entityClipboardRoots.emplace_back(clipboardId, GetEntityParent(id));
  }
  spdlog::info("[Editor] Copied {} entities", entityClipboardRoots.size());
}
auto Editor::PasteEntities() -> void {
  auto scene = GetScene();
  if (!scene || entityClipboardRoots.empty())
    return;
  context.selectedEntities.clear();
  context.selectedEntityId = EntityID::Invalid;
  for (const auto &[clipboardId, originalParent] : entityClipboardRoots) {
    const auto newId = scene->CopyEntityFrom(entityClipboard, clipboardId);
    if (!newId)
      continue;
    if (originalParent && IsEntity(originalParent))
      AddChildEntity(originalParent, newId);
    context.selectedEntities.insert(newId);
    context.selectedEntityId = newId;
  }
  spdlog::info("[Editor] Pasted {} entities", context.selectedEntities.size());
}
static auto IsHandleComponent(const ComponentType type) -> bool {
  switch (type) {
  case ComponentType::MaterialHandle:
  case ComponentType::MeshHandle:
  case ComponentType::ModelMaterialHandle:
  case ComponentType::ModelMeshHandle:
  case ComponentType::SkyboxHandle:
  case ComponentType::TextureHandle:
    return true;
  default:
    return false;
  }
}
auto Editor::DisplayProfileNode(const size_t index) -> void {
  const auto &nodes = Profiler::Get().GetNodes();
  const auto &node = nodes[index];
  auto childMillis = .0;
  for (const auto child : node.children)
    childMillis += nodes[child].lastMillis;
  const auto children = node.children;
  const auto cold = node.lastCalls == 0;
  auto flags = ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_DefaultOpen;
  if (children.empty())
    flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet | ImGuiTreeNodeFlags_NoTreePushOnOpen;
  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  if (cold)
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
  const auto open = ImGui::TreeNodeEx(node.name.c_str(), flags);
  ImGui::TableNextColumn();
  ImGui::Text("%u", node.lastCalls);
  ImGui::TableNextColumn();
  ImGui::Text("%.3f", node.lastMillis);
  ImGui::TableNextColumn();
  ImGui::Text("%.3f", node.lastMillis - childMillis);
  ImGui::TableNextColumn();
  ImGui::Text("%.3f", node.avgMillis);
  ImGui::TableNextColumn();
  ImGui::Text("%.3f", node.maxMillis);
  ImGui::TableNextColumn();
  if (node.sampledFrames > 0)
    ImGui::Text("%llu", static_cast<unsigned long long>(node.maxFrame));
  else
    ImGui::TextUnformatted("-");
  if (cold)
    ImGui::PopStyleColor();
  if (!open)
    return;
  for (const auto child : children)
    DisplayProfileNode(child);
  if (!children.empty())
    ImGui::TreePop();
}
auto Editor::DisplayMenuBar() -> void {
  static constexpr auto BAR_FLAGS = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_AlwaysAutoResize;
  // The keys first and from here, so that they answer whether or not the bar is being looked at.
  // Read before the buttons are drawn and acted on the same way, so the two cannot disagree.
  const auto togglePlay = IsBindingPressed(ShortcutName(PlayStop));
  const auto togglePause = IsBindingPressed(ShortcutName(PauseResume));
  const auto step = IsBindingPressed(ShortcutName(ShortcutIndex::StepTick));
  const auto state = GetPlayState();
  // Tinted for as long as the scene is not the one that will be kept. Anything changed while the
  // bar is this colour is gone at stop, and the colour is the only thing that says so.
  //
  // As see-through as the panels, whatever the colour, since it lies over the game view the same
  // way they do.
  //
  // An ordinary window pinned to the top left corner and sized by what is in it, and not Dear
  // ImGui's main menu bar. That one runs the full width of the viewport however little it holds,
  // which is a strip across the top of the picture for the sake of a few buttons at one end.
  const auto colour = StateTint(state != PlayState::Stopped, state == PlayState::Paused);
  const auto *viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->Pos);
  ImGui::PushStyleColor(ImGuiCol_WindowBg, colour);
  ImGui::Begin("##MenuBar", nullptr, BAR_FLAGS);
  ImGui::PopStyleColor();
  if (ImGui::Button(ICON_LC_PANELS_TOP_LEFT " View"))
    ImGui::OpenPopup("##View");
  // Help is a panel like the rest and is opened from a button of its own beside View rather than
  // from inside it: it is not a view of the scene, and it is the one somebody looks for by name.
  WindowLayout *help{};
  for (auto &layout : gWindowLayouts)
    if (std::string_view(layout.name) == "Help")
      help = &layout;
  if (ImGui::BeginPopup("##View")) {
    for (auto &layout : gWindowLayouts)
      if (&layout != help)
        ImGui::MenuItem((std::string(layout.icon) + " " + layout.name).c_str(), nullptr, &layout.open);
    ImGui::EndPopup();
  }
  ImGui::SameLine();
  if (ImGui::Button(ICON_LC_CIRCLE_QUESTION_MARK " Help") && help)
    help->open = !help->open;
  ImGui::SameLine();
  // One button for play, pause and resume, since only one of them ever applies: it says what
  // pressing it would do now. Either key presses it -- the play key from stopped, the pause key
  // from then on, where the play key means stop.
  const auto *toggle = state == PlayState::Stopped ? ICON_LC_PLAY " Play###PlayPause" : state == PlayState::Playing ? ICON_LC_PAUSE " Pause###PlayPause" : ICON_LC_PLAY " Resume###PlayPause";
  const auto pressed = ImGui::Button(toggle);
  ImGui::SetItemTooltip("Play (%s), pause or resume (%s)", GetTriggerName(GetBinding(ShortcutName(PlayStop))).c_str(), GetTriggerName(GetBinding(ShortcutName(PauseResume))).c_str());
  const auto play = state == PlayState::Stopped && (pressed || togglePlay || togglePause);
  const auto pause = state != PlayState::Stopped && (pressed || togglePause);
  // Only what can be pressed is there to press. Stepping is something done to a paused game and
  // stopping to one that has been started, so neither button exists until its moment; shown
  // greyed out they were two things to read past on a bar that mostly says Play. The keys answer
  // in exactly the states the buttons are shown in.
  auto stepped = step && state == PlayState::Paused;
  if (state == PlayState::Paused) {
    ImGui::SameLine();
    stepped |= ImGui::Button(ICON_LC_STEP_FORWARD " Step");
    ImGui::SetItemTooltip("Run one tick (%s)", GetTriggerName(GetBinding(ShortcutName(ShortcutIndex::StepTick))).c_str());
  }
  auto stop = togglePlay && state != PlayState::Stopped;
  if (state != PlayState::Stopped) {
    ImGui::SameLine();
    stop |= ImGui::Button(ICON_LC_SQUARE " Stop");
    ImGui::SetItemTooltip("Stop and put the scene back (%s)", GetTriggerName(GetBinding(ShortcutName(PlayStop))).c_str());
  }
  if (play)
    BeginPlay();
  else if (pause)
    SetPlayState(state == PlayState::Paused ? PlayState::Playing : PlayState::Paused);
  else if (stepped)
    StepTick();
  else if (stop)
    EndPlay();
  if (state != PlayState::Stopped) {
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    // Drawn in a slot as wide as the longest count it is likely to show, so nothing beside it is
    // pushed about as the digits change sixty times a second -- the digits of this font are not all
    // one width, and a count that grew by one could be narrower than the one before it.
    const auto slot = ImGui::CalcTextSize("tick 00000000").x;
    const auto left = ImGui::GetCursorPosX();
    ImGui::Text("tick %llu", static_cast<unsigned long long>(TickIndex()));
    ImGui::SameLine(left + slot);
    ImGui::Dummy(ImVec2(.0f, .0f));
  }
  ImGui::End();
}
auto Editor::DisplayHelp() -> void {
  if (!BeginWindow("Help")) {
    EndWindow();
    return;
  }
  // Each licence is folded away under the line that names what it is for, and read from disk the
  // first time its fold is opened: there are fifteen of them and most are never looked at. One slot
  // past the credits is the editor's own.
  static std::array<std::string, std::size(kCredits) + 1> texts;
  const auto Licence = [](const char *name, std::string &text) {
    if (text.empty()) {
      std::ifstream file(ResolvePath(std::string("licenses/") + name + ".txt"));
      text = file ? std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()) : std::string("The licence for ") + name + " was not found beside the editor.";
    }
    ImGui::TextUnformatted(text.c_str(), text.c_str() + text.size());
  };
  ImGui::SeparatorText("Kuki Editor");
  // What it is down the left and the answer down the right. `LabelText` would do this in one call
  // and the other way round, value first.
  const auto width = ImGui::GetContentRegionAvail().x;
  const auto Row = field::Row;
  Row("Version", KUKI_VERSION);
#ifdef NDEBUG
  Row("Build", "Release");
#else
  Row("Build", "Debug");
#endif
#ifdef KUKI_HAS_DIRECTX
  Row("Graphics", dynamic_cast<DXContext *>(GetGraphicsContext()) ? "Direct3D 12" : "OpenGL");
#else
  Row("Graphics", "OpenGL");
#endif
  if (field::Fold("Licence: MIT", false, false))
    Licence("Kuki", texts.back());
  ImGui::SeparatorText("Made with");
  // The name is the fold, and what it is used for and on what terms follow it on the same line, at
  // the same two places down the whole list so that they read as columns.
  for (size_t i = 0; i < std::size(kCredits); ++i) {
    const auto &credit = kCredits[i];
    const auto open = field::Fold(credit.name, false, false);
    ImGui::SameLine(width * .3f);
    ImGui::TextDisabled("%s", credit.use);
    field::RightAlign(ImGui::CalcTextSize(credit.licence).x);
    ImGui::TextDisabled("%s", credit.licence);
    if (open)
      Licence(credit.name, texts[i]);
  }
  EndWindow();
}
auto Editor::IsSupported(const ComponentType type) const -> bool {
  switch (type) {
  case ComponentType::AtmosphereSky:
    return context.capabilities.proceduralSky;
  case ComponentType::VolumetricClouds:
  case ComponentType::VolumetricFog:
    return context.capabilities.volumetrics;
  case ComponentType::ParticleEmitter:
    return context.capabilities.particles;
  case ComponentType::Stylization:
  case ComponentType::ScreenEffects:
  case ComponentType::PlanarReflection:
    return context.capabilities.stylization;
  case ComponentType::Wind:
    return context.capabilities.wind;
  default:
    return true;
  }
}
auto Editor::BeginPlay() -> void {
  if (GetPlayState() != PlayState::Stopped)
    return;
  std::error_code error;
  playSnapshot = std::filesystem::temp_directory_path(error) / "kuki-editor-play.json";
  if (error || !SceneSerializer::Save(*this, playSnapshot)) {
    // Not played at all rather than played with nothing to go back to.
    spdlog::error("[Editor] The scene could not be saved to {}, so it was not played", playSnapshot.string());
    playSnapshot.clear();
    return;
  }
  SetPlayState(PlayState::Playing);
}
auto Editor::EndPlay() -> void {
  if (GetPlayState() == PlayState::Stopped)
    return;
  SetPlayState(PlayState::Stopped);
  if (!playSnapshot.empty())
    LoadScene(playSnapshot);
}
auto Editor::DisplayProfiler() -> void {
  static constexpr auto TABLE_FLAGS = ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_ScrollY;
  auto &profiler = Profiler::Get();
  const auto recording = profiler.IsEnabled();
  const auto paused = profiler.IsPaused();
  // Green while it records and amber while it is held, as the menu bar is for the game.
  ImGui::PushStyleColor(ImGuiCol_WindowBg, StateTint(recording, paused));
  const auto visible = BeginWindow("Profiler");
  ImGui::PopStyleColor();
  if (!visible) {
    EndWindow();
    return;
  }
  // Laid out as the game's controls are: one button to begin with, and the rest only once there
  // is something for them to act on. Stopping also lets go of a pause, so that the next recording
  // starts running rather than held from the last one.
  if (!recording) {
    if (ImGui::Button(ICON_LC_CIRCLE " Record"))
      profiler.SetEnabled(true);
  } else {
    if (ImGui::Button(paused ? ICON_LC_PLAY " Resume###PauseResume" : ICON_LC_PAUSE " Pause###PauseResume"))
      profiler.SetPaused(!paused);
    ImGui::SameLine();
    if (ImGui::Button(ICON_LC_ROTATE_CCW " Reset"))
      profiler.Reset();
    ImGui::SameLine();
    if (ImGui::Button(ICON_LC_SQUARE " Stop")) {
      profiler.SetPaused(false);
      profiler.SetEnabled(false);
    }
  }
  ImGui::SameLine();
  const auto frameMillis = profiler.GetFrameMillis();
  ImGui::TextDisabled("frame %llu | %.2f ms | %.0f FPS", static_cast<unsigned long long>(profiler.GetFrameIndex()), frameMillis, frameMillis > .0f ? 1000.f / frameMillis : .0f);
  const auto &history = profiler.GetFrameHistory();
  ImGui::PlotLines("##FrameTimes", history.data(), static_cast<int>(history.size()), static_cast<int>(profiler.GetFrameHistoryOffset()), nullptr, .0f, std::numeric_limits<float>::max(), ImVec2(-1.f, 48.f));
  if (const auto &marks = profiler.GetMarks(); !marks.empty() && field::Fold("Marks")) {
    ImGui::TextDisabled("recorded on frame %llu", static_cast<unsigned long long>(profiler.GetMarkFrame()));
    for (const auto &mark : marks)
      ImGui::Text("%8.3f ms  %s", mark.millis, mark.name.c_str());
  }
  if (!profiler.IsEnabled())
    ImGui::TextDisabled("Profiling is off. Nothing is being recorded.");
  else if (profiler.IsPaused())
    ImGui::TextDisabled("Paused. The numbers below are frozen where they were.");
  const auto tableHeight = std::max(ImGui::GetContentRegionAvail().y, ImGui::GetTextLineHeightWithSpacing() * 4.f);
  if (ImGui::BeginTable("ProfilerScopes", 7, TABLE_FLAGS, ImVec2(.0f, tableHeight))) {
    ImGui::TableSetupColumn("Scope", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Calls", ImGuiTableColumnFlags_WidthFixed, 48.f);
    ImGui::TableSetupColumn("Last (ms)", ImGuiTableColumnFlags_WidthFixed, 72.f);
    ImGui::TableSetupColumn("Self (ms)", ImGuiTableColumnFlags_WidthFixed, 72.f);
    ImGui::TableSetupColumn("Avg (ms)", ImGuiTableColumnFlags_WidthFixed, 72.f);
    ImGui::TableSetupColumn("Max (ms)", ImGuiTableColumnFlags_WidthFixed, 72.f);
    ImGui::TableSetupColumn("Peak frame", ImGuiTableColumnFlags_WidthFixed, 80.f);
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableHeadersRow();
    for (const auto root : profiler.GetRoots())
      DisplayProfileNode(root);
    ImGui::EndTable();
  }
  EndWindow();
}
auto Editor::DisplayProperties() -> void {
  const auto expanded = BeginWindow("Properties");
  if (!expanded || !context.selectedEntityId) {
    if (expanded)
      ImGui::TextDisabled("Select an entity to edit its components.");
    EndWindow();
    return;
  }
  auto componentTypes = GetEntityComponentTypes(context.selectedEntityId);
  for (auto i = 0; i < componentTypes.size(); ++i) {
    const auto componentType = componentTypes[i];
    if (IsHandleComponent(componentType))
      continue;
    ImGui::PushID(static_cast<int>(i));
    const auto name = Component::GetTypeName(componentType);
    // A component the running backend cannot act on is headed in orange. Its section is empty --
    // there is nothing to edit that would reach anything -- and without a colour that would look
    // like a component with no properties, or one that had failed to draw. The New menu does not
    // offer these, but a scene file carries them onto whichever backend opens it.
    const auto unsupported = !IsSupported(componentType);
    if (unsupported) {
      const auto alpha = ImGui::GetStyleColorVec4(ImGuiCol_Header).w;
      ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(UNSUPPORTED_TINT.x, UNSUPPORTED_TINT.y, UNSUPPORTED_TINT.z, alpha));
      ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(UNSUPPORTED_TINT.x * 1.15f, UNSUPPORTED_TINT.y * 1.15f, UNSUPPORTED_TINT.z * 1.15f, alpha));
      ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(UNSUPPORTED_TINT.x * 1.3f, UNSUPPORTED_TINT.y * 1.3f, UNSUPPORTED_TINT.z * 1.3f, alpha));
    }
    const auto open = field::Fold(IconLabel(ComponentIcon(componentType), name).c_str(), true);
    if (unsupported) {
      ImGui::PopStyleColor(3);
      ImGui::SetItemTooltip("The graphics backend that is running cannot act on this component.");
    }
    if (open) {
      auto componentOpt = GetEntityComponent(context.selectedEntityId, componentType);
      if (componentOpt.has_value())
        DisplayProperties(componentOpt.value());
    }
    ImGui::PopID();
  }
  EndWindow();
}
auto Editor::DisplayProperties(const ComponentVariant &variant) -> void {
  PropertyDisplayer displayer{context, *this};
  std::visit(displayer, variant);
}
auto Editor::DisplayScene() -> void {
  // The game view is the backdrop the panels float over: it covers the whole viewport, has no
  // decoration of its own, and never comes to the front when clicked.
  static constexpr auto SCENE_WINDOW_FLAGS = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
  static constexpr ImVec2 FLIP_UV0(0.f, 1.f);
  static constexpr ImVec2 FLIP_UV1(1.f, 0.f);
  static constexpr ImVec2 UV0(0.f, 0.f);
  static constexpr ImVec2 UV1(1.f, 1.f);
  static constexpr float PICK_DRAG_THRESHOLD_SQ = 4.f * 4.f;
  const auto *viewport = ImGui::GetMainViewport();
  // The whole viewport, always. The menu bar floats over a corner of the picture as the panels
  // float over its sides, and the picture is never resized to make room for any of them.
  ImGui::SetNextWindowPos(viewport->Pos);
  ImGui::SetNextWindowSize(viewport->Size);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(.0f, .0f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, .0f);
  // The panels are translucent, the backdrop must not be: nothing renders behind the game view.
  ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(.0f, .0f, .0f, 1.f));
  ImGui::Begin("Scene", nullptr, SCENE_WINDOW_FLAGS);
  ImGui::PopStyleColor();
  ImGui::PopStyleVar(2);
  SetViewportHovered(ImGui::IsWindowHovered());
  if (IsBindingPressed(ShortcutName(ToggleFPS)))
    context.showFPS = !context.showFPS;
  auto renderingSystem = GetSystem<RenderingSystem>();
  if (renderingSystem) {
    const auto &contentRegion = ImGui::GetContentRegionAvail();
    const auto sceneWidth = static_cast<int>(contentRegion.x);
    const auto sceneHeight = static_cast<int>(contentRegion.y);
    SetResolution(sceneWidth, sceneHeight);
    const auto sceneTarget = renderingSystem->GetTarget(context.debugViewTarget);
    if (sceneTarget && sceneTarget->GetTextureHandle() > 0) {
      const auto res = renderingSystem->GetResolution();
      const auto flip = sceneTarget->NeedsVerticalFlip();
      ImGui::Image(static_cast<ImTextureID>(sceneTarget->GetTextureHandle()), ImVec2(res.first, res.second), flip ? FLIP_UV0 : UV0, flip ? FLIP_UV1 : UV1);
      const auto imageMin = ImGui::GetItemRectMin();
      const auto imageMax = ImGui::GetItemRectMax();
      if (ImGui::IsItemHovered() && IsInputPressed(GLFW_MOUSE_BUTTON_LEFT) && !ImGuizmo::IsOver() && !ImGuizmo::IsUsing()) {
        pickPressPos = GetMousePosition();
        trackingClick = true;
      }
      if (trackingClick && IsInputReleased(GLFW_MOUSE_BUTTON_LEFT)) {
        trackingClick = false;
        const auto releasePos = GetMousePosition();
        const auto dx = releasePos.x - pickPressPos.x;
        const auto dy = releasePos.y - pickPressPos.y;
        const auto localX = releasePos.x - imageMin.x;
        const auto localY = releasePos.y - imageMin.y;
        const auto sizeX = imageMax.x - imageMin.x;
        const auto sizeY = imageMax.y - imageMin.y;
        if (dx * dx + dy * dy < PICK_DRAG_THRESHOLD_SQ && sizeX > 0.f && sizeY > 0.f && localX >= 0.f && localY >= 0.f && localX < sizeX && localY < sizeY) {
          const auto texelX = static_cast<int>(localX / sizeX * res.first);
          const auto texelY = static_cast<int>(localY / sizeY * res.second);
          const auto pickedId = renderingSystem->PickEntity(texelX, texelY);
          if (context.keyState.test(static_cast<uint8_t>(KeyBit::Shift))) {
            if (pickedId) {
              if (context.selectedEntities.contains(pickedId)) {
                context.selectedEntities.erase(pickedId);
                context.selectedEntityId = EntityID::Invalid;
              } else {
                context.selectedEntities.insert(pickedId);
                context.selectedEntityId = pickedId;
              }
            }
          } else {
            context.selectedEntities.clear();
            if (pickedId) {
              context.selectedEntities.insert(pickedId);
              context.selectedEntityId = pickedId;
            } else
              context.selectedEntityId = EntityID::Invalid;
          }
        }
      }
      DrawManipulator(res.first, res.second);
    }
  }
  if (ImGui::BeginDragDropTarget()) {
    if (auto payload = ImGui::AcceptDragDropPayload("##SpawnPayload")) {
      const auto assetIdPtr = static_cast<const AssetID *>(payload->Data);
      InstantiateAsset(*assetIdPtr);
      ImGui::SetWindowFocus();
    }
    ImGui::EndDragDropTarget();
  }
  if (context.showFPS) {
    // The game view has no padding of its own, so this lands in the very corner of the viewport.
    // The bottom left one: the top left is where the menu bar is, and the panels keep to the right.
    ImGui::SetCursorPos(ImVec2(.0f, ImGui::GetWindowHeight() - ImGui::GetTextLineHeight()));
    ImGui::Text("%zu", GetFPS());
  }
  ImGui::End();
}
auto Editor::DisplaySettings() -> void {
  if (!BeginWindow("Settings")) {
    EndWindow();
    return;
  }
  field::Toggle(ICON_LC_GAUGE, "Show FPS", &context.showFPS);
  if (field::Fold(IconLabel(ICON_LC_VIDEO, "Camera Controller").c_str(), false))
    GetSystem<CameraController>()->Display();
  auto previewSize = GetPreviewSize();
  if (field::SliderInt("Asset Preview Size", &previewSize, 32, 256, "%d", ImGuiSliderFlags_AlwaysClamp))
    SetPreviewSize(previewSize);
  if (auto renderingSystem = GetSystem<RenderingSystem>(); renderingSystem && field::BeginCombo("Debug View", context.debugViewTarget.empty() ? "Final" : context.debugViewTarget.c_str())) {
    if (ImGui::Selectable("Final", context.debugViewTarget.empty()))
      context.debugViewTarget.clear();
    renderingSystem->ForEachTarget([this](const std::string &name, const TargetDescription &desc) {
      if (desc.type != TargetType::Texture2D)
        return;
      if (ImGui::Selectable(name.c_str(), context.debugViewTarget == name))
        context.debugViewTarget = name;
    });
    ImGui::EndCombo();
  }
  ImGui::SetItemTooltip("Which render graph target is shown in place of the final image.\nThese are the steps of the lighting that end in a resource;\nthe camera's two views are the ones that do not.");
  if (auto renderingSystem = GetSystem<RenderingSystem>(); renderingSystem) {
    if (context.capabilities.lightingDebugViews || context.capabilities.probeVolume) {
      ImGui::TextDisabled("Shading and probe views are on the Camera.");
      ImGui::SetItemTooltip("A debug view is a way of looking, so it belongs to the thing that looks.\nSelect a camera in the hierarchy to set its view, and keep a second camera\non a different one to switch between them by switching camera.");
    } else {
      // Said once, here, rather than left for somebody to work out from a combo box that is not
      // there. The targets above are the whole of what this backend can be asked to draw in place
      // of the picture, and knowing that is the difference between a limitation and a suspected
      // fault.
      ImGui::TextDisabled("This backend has no shading or probe views.");
      ImGui::SetItemTooltip("Those are values inside the scene shader, and this one writes finished\npixels only. The targets above are every step of the frame it can show.");
    }
    // Said here because this is where it will be looked for. The fold below has an AntiAliasing
    // checkbox, and that stands the resolve down rather than changing how many samples there were
    // to resolve -- a different question with the same word on it. How many samples describes the
    // scene rather than the installation, so it sits with the scene's other renderer settings and
    // not in this panel.
    ImGui::TextDisabled("How much antialiasing is on the Settings entity.");
    ImGui::SetItemTooltip("Select it in the hierarchy to change the sample count. The checkbox in\nthe fold below is a different control: it stands the resolve pass down,\nrather than changing how many samples there were to resolve.");
    if (field::Fold("Passes")) {
      // Collapsed by default. Twelve checkboxes is a lot to put in front of somebody who came here
      // to change the tone curve, and the reason to open it -- finding out what a pass is worth by
      // taking it away -- is a thing you go looking for rather than come across.
      const auto &passNames = EnumTraits<RenderPass>::GetNames();
      for (size_t i = 0; i < passNames.size(); ++i) {
        const auto pass = static_cast<RenderPass>(i);
        // The trace is in the graph on every backend, because the scene pass orders against its
        // output whether or not anything fills it. Only a backend that fills it gets the checkbox:
        // standing down a pass that does nothing is a control with nothing on the other end.
        if (pass == RenderPass::ProbeTrace && !context.capabilities.probeVolume)
          continue;
        auto enabled = renderingSystem->IsPassEnabled(pass);
        if (field::Toggle(ICON_LC_EYE, passNames[i], &enabled))
          renderingSystem->SetPassEnabled(pass, enabled);
      }
      ImGui::SetItemTooltip("A pass that is off still runs a stand-in, so the passes after it are\nhanded something usable rather than the picture from last frame.\nMostly that means the input passed through untouched.");
      if (const auto usage = renderingSystem->GetPoolUsage(); usage.inUse || usage.available) {
        ImGui::Separator();
        ImGui::Text("Pools: %zu lent, %zu waiting, %zu kinds", usage.inUse, usage.available, usage.keys);
        ImGui::SetItemTooltip("Reusable textures, buffers and framebuffers the backend is holding.\nWhat is waiting is trimmed back to recent demand every few seconds,\nand a size nothing has asked for in a while is released outright.");
      }
    }
  }
  ImGui::Separator();
  ImGui::TextUnformatted("Graphics");
  const auto activeApi = GetDescription().api;
  if (field::BeginCombo("Rendering API", std::string(ToString(context.pendingApi)).c_str())) {
    for (const auto api : {RenderingAPI::OpenGL, RenderingAPI::DirectX, RenderingAPI::Vulkan}) {
      const auto available = IsAvailable(api);
      ImGui::BeginDisabled(!available);
      const auto label = std::string(ToString(api)) + (available ? "" : " (unavailable)");
      if (ImGui::Selectable(label.c_str(), context.pendingApi == api) && available) {
        context.pendingApi = api;
        SaveGraphicsConfig();
      }
      ImGui::EndDisabled();
    }
    ImGui::EndCombo();
  }
  if (auto renderingSystem = GetSystem<RenderingSystem>(); renderingSystem) {
    const auto &toneMapperNames = EnumTraits<ToneMapper>::GetNames();
    const auto activeToneMapper = renderingSystem->GetToneMapper();
    if (field::BeginCombo("Tone Mapping", toneMapperNames[static_cast<size_t>(activeToneMapper)])) {
      for (size_t i = 0; i < toneMapperNames.size(); ++i)
        if (ImGui::Selectable(toneMapperNames[i], static_cast<size_t>(activeToneMapper) == i)) {
          renderingSystem->SetToneMapper(static_cast<ToneMapper>(i));
          SaveGraphicsConfig();
        }
      ImGui::EndCombo();
    }
    ImGui::SetItemTooltip("Curve the scene's radiance is fitted onto the display with.\nNone clips and is the reference to judge the others against;\nACES is punchy and shifts bright saturated colour towards yellow;\nAgX holds hue and desaturates towards white instead.");
    ImGui::TextDisabled("Exposure is on the active camera.");
    ImGui::SetItemTooltip("The curve is a property of the display you are working at, so it lives here.\nThe exposure feeding it belongs to the shot, so it lives on the Camera\ncomponent and is saved with the scene.");
    if (const auto budget = renderingSystem->GetUploadBudget(); budget > 0) {
      auto megabytes = static_cast<int>(budget / (1024 * 1024));
      if (field::SliderInt("Texture Upload Batch", &megabytes, 16, 512, "%d MB", ImGuiSliderFlags_AlwaysClamp))
        renderingSystem->SetUploadBudget(static_cast<size_t>(megabytes) * 1024 * 1024);
      ImGui::SetItemTooltip("Texture staging held in flight before the upload path waits for the GPU.\nLarger batches mean fewer waits while a model loads and more memory held while it does.");
    }
  }
  if (context.pendingApi == activeApi)
    ImGui::TextDisabled("Active: %s", std::string(ToString(activeApi)).c_str());
  else
    ImGui::TextColored(ImVec4(1.f, .8f, .2f, 1.f), "Restart to switch from %s to %s", std::string(ToString(activeApi)).c_str(), std::string(ToString(context.pendingApi)).c_str());
  ImGui::Separator();
  ImGui::Text("Shortcuts");
  const auto &bindingNames = GetBindingNames();
  std::string currentCategory;
  for (auto i = 0; i < static_cast<int>(bindingNames.size()); ++i) {
    const auto &name = bindingNames[i];
    const auto dot = name.find('.');
    const auto category = dot == std::string::npos ? std::string() : name.substr(0, dot);
    const auto label = dot == std::string::npos ? name : name.substr(dot + 1);
    if (category != currentCategory) {
      currentCategory = category;
      ImGui::SeparatorText(currentCategory.c_str());
    }
    DisplayKeyBindingRow(*this, label.c_str(), GetBindingDescription(name), name, shortcutRebindingIndex, i);
  }
  ImGui::Separator();
  ImGui::Text("Sequences");
  for (const auto &sequence : GetSequenceNames())
    DisplaySequenceRow(sequence, GetSequenceDescription(sequence));
  EndWindow();
}
auto Editor::DrawManipulator(const float width, const float height) -> void {
  if (context.selectedEntities.empty())
    return;
  auto *cameraController = GetSystem<CameraController>();
  if (!cameraController->GetEntity() || !IsEntity(cameraController->GetEntity()))
    AttachCameraController();
  const auto windowPos = ImGui::GetWindowPos();
  ImGuizmo::SetRect(windowPos.x, windowPos.y, width, height);
  ImGuizmo::SetOrthographic(cameraController->GetType() == CameraType::Orthographic);
  struct ManipulatorTarget {
    Transform *transformComp{};
    Camera *cameraComp{};
    Light *lightComp{};
    Transform current{};
  };
  std::vector<ManipulatorTarget> targets;
  targets.reserve(context.selectedEntities.size());
  for (const auto &id : context.selectedEntities) {
    ManipulatorTarget target;
    target.transformComp = GetEntityComponent<Transform>(id);
    if (target.transformComp)
      target.current = *target.transformComp;
    else {
      target.cameraComp = GetEntityComponent<Camera>(id);
      if (target.cameraComp)
        target.current = target.cameraComp->GetTransform();
      else {
        target.lightComp = GetEntityComponent<Light>(id);
        if (target.lightComp)
          target.current = target.lightComp->GetTransform();
        else
          continue;
      }
    }
    targets.push_back(target);
  }
  if (targets.empty())
    return;
  glm::mat4 pivot;
  if (targets.size() == 1)
    pivot = targets[0].current.world;
  else {
    glm::vec3 centroid{};
    for (const auto &target : targets)
      centroid += glm::vec3(target.current.world[3]);
    centroid /= static_cast<float>(targets.size());
    pivot = glm::translate(glm::mat4(1.f), centroid);
  }
  const auto pivotBefore = pivot;
  ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
  if (!ImGuizmo::Manipulate(glm::value_ptr(cameraController->GetView()), glm::value_ptr(cameraController->GetProjection()), ImGuizmo::OPERATION::UNIVERSAL, ImGuizmo::MODE::WORLD, glm::value_ptr(pivot)))
    return;
  const auto delta = pivot * glm::inverse(pivotBefore);
  for (auto &target : targets) {
    const auto newWorld = delta * target.current.world;
    glm::vec3 rotation;
    Transform result;
    ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(newWorld), glm::value_ptr(result.position), glm::value_ptr(rotation), glm::value_ptr(result.scale));
    result.rotation = glm::quat(glm::radians(rotation));
    result.world = newWorld;
    if (target.cameraComp)
      target.cameraComp->SetTransform(result);
    else if (target.lightComp)
      target.lightComp->SetTransform(result);
    else
      *target.transformComp = result;
  }
}
