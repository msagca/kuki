// `SettingsMenu::AddLightingEntries`, apart from the menu itself because it is a list of the lighting's
// values rather than anything about menus.
#include <application.hpp>
#include <camera.hpp>
#include <cstddef>
#include <cstdint>
#include <debug_view.hpp>
#include <enum_traits.hpp>
#include <functional>
#include <indirect_lighting.hpp>
#include <renderer_capabilities.hpp>
#include <rendering_system.hpp>
#include <settings_menu.hpp>
#include <string>
#include <vector>
namespace kuki {
namespace {
auto Options(const std::vector<const char *> &names) -> std::vector<SettingsOption> {
  std::vector<SettingsOption> options;
  for (const auto *name : names)
    options.push_back({name, {}});
  return options;
}
} // namespace
auto SettingsMenu::AddLightingEntries(const SettingsPage target) -> void {
  const auto *rendering = app.GetRenderingSystem();
  const auto capabilities = rendering ? rendering->GetCapabilities() : RendererCapabilities{};
  const auto present = [this] { return app.GetSceneSingleton<IndirectLighting>() != nullptr; };
  // One row for one field of the scene's `IndirectLighting`, read and written through the scene each
  // time rather than held, so a scene loaded later is the one being tuned.
  const auto Value = [&](std::string label, float IndirectLighting::*field, const SettingsRange range) {
    AddNumber(target, std::move(label), range, [this, field] {
      const auto *lighting = app.GetSceneSingleton<IndirectLighting>();
      return lighting ? lighting->*field : IndirectLighting{}.*field; }, [this, field](const float value) {
      if (auto *lighting = app.GetSceneSingleton<IndirectLighting>())
        lighting->*field = value; }, present);
  };
  // The terms first: how much bounce and sky reach the picture, which is the first question asked
  // of indirect light and usually the only one. The sky's and the fallback are any backend's.
  if (capabilities.probeVolume)
    Value("Bounce", &IndirectLighting::bounceIntensity, {.0f, 4.f, .05f, 2});
  Value("Sky", &IndirectLighting::skyIntensity, {.0f, 4.f, .05f, 2});
  Value("Ambient fallback", &IndirectLighting::ambientFallback, {.0f, .5f, .005f, 3});
  if (capabilities.probeVolume) {
    // How the field is read per pixel: moving one repaints the next frame.
    Value("Surface bias", &IndirectLighting::probeSurfaceBias, {.0f, 1.f, .01f, 2});
    Value("Weight floor", &IndirectLighting::probeWeightFloor, {.0f, 1.f, .01f, 2});
    Value("Visibility sharpness", &IndirectLighting::probeVisibilitySharpness, {1.f, 8.f, .1f, 1});
    // How the field is filled: moving one winds the running mean back, and it settles again.
    Value("Depth sharpness", &IndirectLighting::probeDepthSharpness, {4.f, 64.f, 1.f, 0});
    Value("Depth range", &IndirectLighting::probeDepthRange, {.05f, 2.f, .05f, 2});
    Value("Ray distance", &IndirectLighting::rayDistanceScale, {.25f, 4.f, .05f, 2});
    Value("Hit normal nudge", &IndirectLighting::hitNormalNudge, {.0f, 1.f, .01f, 2});
    Value("Opaque threshold", &IndirectLighting::opaqueThreshold, {.005f, .5f, .005f, 3});
    AddNumber(target, "Mean step", {1.f, 16.f, 1.f, 0}, [this] {
      const auto *lighting = app.GetSceneSingleton<IndirectLighting>();
      return static_cast<float>(lighting ? lighting->meanStep : IndirectLighting{}.meanStep); }, [this](const float value) {
      if (auto *lighting = app.GetSceneSingleton<IndirectLighting>())
        lighting->meanStep = static_cast<uint32_t>(value); }, present);
    // Where the probes stand, and when one is disbelieved.
    Value("Relocation margin", &IndirectLighting::relocationMargin, {.0f, 1.f, .05f, 2});
    Value("Relocation damping", &IndirectLighting::relocationDamping, {.0f, 1.f, .05f, 2});
    Value("Escape agreement", &IndirectLighting::escapeAgreement, {.01f, 1.f, .01f, 2});
    Value("Buried limit", &IndirectLighting::buriedLimit, {.01f, 1.f, .01f, 2});
  }
  // The views are the camera's, since they are a way of looking rather than part of the scene.
  const auto camera = [this] { return app.GetCamera() != nullptr; };
  if (capabilities.lightingDebugViews || capabilities.probeVolume)
    AddChoice(target, "Shading view", Options(EnumTraits<LightingDebugView>::GetNames()), [this] {
      const auto *view = app.GetCamera();
      return view ? static_cast<size_t>(view->lightingDebugView) : size_t{0}; }, [this](const size_t index) {
      if (auto *view = app.GetCamera())
        view->lightingDebugView = static_cast<LightingDebugView>(index); }, camera);
  if (capabilities.probeVolume)
    AddChoice(target, "Probe view", Options(EnumTraits<ProbeDebugView>::GetNames()), [this] {
      const auto *view = app.GetCamera();
      return view ? static_cast<size_t>(view->probeDebugView) : size_t{0}; }, [this](const size_t index) {
      if (auto *view = app.GetCamera())
        view->probeDebugView = static_cast<ProbeDebugView>(index); }, camera);
  // Back to what the scene was built with, which is a game's own choice of values and not the
  // engine's defaults: the frontier's sky is brighter than the default on purpose.
  const auto *built = app.GetSceneSingleton<IndirectLighting>();
  AddAction(target, "Reset lighting", [this, start = built ? *built : IndirectLighting{}] {
    if (auto *lighting = app.GetSceneSingleton<IndirectLighting>())
      *lighting = start;
    if (auto *view = app.GetCamera()) {
      view->lightingDebugView = LightingDebugView::None;
      view->probeDebugView = ProbeDebugView::Off;
    } }, present);
}
} // namespace kuki
