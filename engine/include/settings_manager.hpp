#pragma once
#include <application_settings.hpp>
#include <event.hpp>
#include <kuki_engine_export.h>
namespace kuki {
class KUKI_ENGINE_API SettingsManager {
public:
  Event<ScreenResolution> OnResolutionChanged;
  auto GetResolution() const -> const ScreenResolution &;
  auto GetSettings() const -> const ApplicationSettings &;
  auto SetResolution(const ScreenResolution &) -> void;
  auto SetResolution(int, int) -> void;
private:
  ApplicationSettings settings;
};
} // namespace kuki
