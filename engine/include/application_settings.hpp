#pragma once
namespace kuki {
struct ScreenResolution {
  int width{1920};
  int height{1080};
};
struct ApplicationSettings {
  ScreenResolution res;
};
} // namespace kuki
