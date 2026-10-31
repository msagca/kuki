#include <cstddef>
#ifdef KUKI_HAS_DIRECTX
#include <dx_common.hpp>
#endif
#include <engine_config.hpp>
#include <enum_traits.hpp>
#include <filesystem>
#include <fstream>
#include <launch_path.hpp>
#include <nlohmann/json.hpp>
#include <post_process.hpp>
#include <rendering_api.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include <string_view>
#include <tone_mapper.hpp>
namespace kuki {
namespace {
constexpr auto CONFIG_FILE_NAME = "kuki.config.json";
/// @brief Reads a tone mapper back by the same name `EnumTraits` writes it under.
///
/// Named rather than numbered on disk, so that inserting an operator does not silently reinterpret
/// every config already written. An unrecognised name keeps the fallback rather than failing the
/// load: a config from a build that had a curve this one does not is worth reading the rest of.
auto ParseToneMapper(const std::string &name, const ToneMapper fallback) -> ToneMapper {
  const auto &names = EnumTraits<ToneMapper>::GetNames();
  for (size_t i = 0; i < names.size(); ++i)
    if (name == names[i])
      return static_cast<ToneMapper>(i);
  return fallback;
}
auto ParseAPI(const std::string &name, const RenderingAPI fallback) -> RenderingAPI {
  if (name == "DirectX")
    return RenderingAPI::DirectX;
  if (name == "Vulkan")
    return RenderingAPI::Vulkan;
  // Said out loud, unlike an unknown tone mapper: a config left from before OpenGL was removed still
  // names it, and someone who chose it should learn why they are not getting it.
  spdlog::warn("[Config] No backend is called {} in this build, using {}", name, ToString(fallback));
  return fallback;
}
} // namespace
auto ToString(const RenderingAPI api) -> std::string_view {
  switch (api) {
  case RenderingAPI::DirectX:
    return "DirectX";
  case RenderingAPI::Vulkan:
    return "Vulkan";
  }
  return "Unknown";
}
auto IsAvailable(const RenderingAPI api) -> bool {
  switch (api) {
  case RenderingAPI::DirectX:
#ifdef KUKI_HAS_DIRECTX
    return DXDeviceAvailable();
#else
    return false;
#endif
  case RenderingAPI::Vulkan:
    return false;
  }
  return false;
}
auto ConfiguredAPI() -> RenderingAPI {
  return EngineConfig::Load().api;
}
auto DefaultAPI() -> RenderingAPI {
  return RenderingAPI::DirectX;
}
auto EngineConfig::GetPath() -> std::filesystem::path {
  return GetLaunchPath() / CONFIG_FILE_NAME;
}
auto EngineConfig::Load() -> EngineConfig {
  EngineConfig config;
  const auto path = GetPath();
  std::error_code error;
  if (!std::filesystem::exists(path, error))
    return config;
  std::ifstream stream(path);
  if (!stream) {
    spdlog::warn("[Config] Could not open {}", path.string());
    return config;
  }
  try {
    const auto json = nlohmann::json::parse(stream);
    if (const auto it = json.find("renderingApi"); it != json.end() && it->is_string())
      config.api = ParseAPI(it->get<std::string>(), config.api);
    if (const auto it = json.find("toneMapper"); it != json.end() && it->is_string())
      config.toneMapper = ParseToneMapper(it->get<std::string>(), config.toneMapper);
    const auto Flag = [&](const char *name, bool &value) {
      if (const auto it = json.find(name); it != json.end() && it->is_boolean())
        value = it->get<bool>();
    };
    Flag("fullscreen", config.fullscreen);
    Flag("vsync", config.vsync);
    Flag("shadows", config.shadows);
    Flag("bloom", config.bloom);
    Flag("volumetrics", config.volumetrics);
    Flag("reflections", config.reflections);
    Flag("showFrameRate", config.showFrameRate);
    // By name, like the tone mapper, so that a level added between two others reads back as itself.
    if (const auto it = json.find("lighting"); it != json.end() && it->is_string()) {
      const auto name = it->get<std::string>();
      config.lighting = name == "low" ? LightingQuality::Low : name == "medium" ? LightingQuality::Medium
                                                                              : LightingQuality::High;
    }
    // By the sample count, which is what the setting means to anyone reading the file; a count the
    // enum has no mode for keeps the default.
    if (const auto it = json.find("antiAliasing"); it != json.end() && it->is_number_integer())
      for (const auto mode : {AntiAliasingMode::None, AntiAliasingMode::MSAA2x, AntiAliasingMode::MSAA4x, AntiAliasingMode::MSAA8x})
        if (SampleCount(mode) == it->get<int>())
          config.antiAliasing = mode;
  } catch (const nlohmann::json::exception &exception) {
    spdlog::warn("[Config] Malformed {}, using defaults: {}", path.string(), exception.what());
    return {};
  }
  // A name this build does not know -- "OpenGL", from before its backend went -- has left the default
  // in place above. An API it knows but cannot run is replaced by the default too, unless it is the
  // default: then nothing can draw, and `GraphicsContext::Create` is the one to say why.
  if (!IsAvailable(config.api) && config.api != DefaultAPI()) {
    spdlog::warn("[Config] {} is not available in this build, using {}", ToString(config.api), ToString(DefaultAPI()));
    config.api = DefaultAPI();
  }
  return config;
}
auto EngineConfig::Save() const -> bool {
  const auto path = GetPath();
  nlohmann::json json;
  json["renderingApi"] = std::string(ToString(api));
  json["toneMapper"] = EnumTraits<ToneMapper>::GetNames()[static_cast<size_t>(toneMapper)];
  json["fullscreen"] = fullscreen;
  json["vsync"] = vsync;
  json["antiAliasing"] = SampleCount(antiAliasing);
  json["shadows"] = shadows;
  json["bloom"] = bloom;
  json["volumetrics"] = volumetrics;
  json["reflections"] = reflections;
  json["lighting"] = lighting == LightingQuality::Low ? "low" : lighting == LightingQuality::Medium ? "medium"
                                                                                                   : "high";
  json["showFrameRate"] = showFrameRate;
  std::ofstream stream(path);
  if (!stream) {
    spdlog::error("[Config] Could not write {}", path.string());
    return false;
  }
  stream << json.dump(2) << '\n';
  spdlog::info("[Config] Saved {}", path.string());
  return true;
}
} // namespace kuki
