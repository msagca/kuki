#include <settings_manager.hpp>
#include <spdlog/spdlog.h>
namespace kuki {
auto SettingsManager::GetResolution() const -> const ScreenResolution & {
  return settings.res;
}
auto SettingsManager::GetSettings() const -> const ApplicationSettings & {
  return settings;
}
auto SettingsManager::SetResolution(const ScreenResolution &res) -> void {
  SetResolution(res.width, res.height);
}
auto SettingsManager::SetResolution(int width, int height) -> void {
  if (width <= 0 || height <= 0 || (settings.res.width == width && settings.res.height == height))
    return;
  settings.res.width = width;
  settings.res.height = height;
  // TODO: maybe defer this to the next frame
  OnResolutionChanged.Emit(settings.res);
}
} // namespace kuki
