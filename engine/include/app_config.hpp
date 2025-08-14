#pragma once
#include <filesystem>
#include <kuki_engine_export.h>
namespace kuki {
struct KUKI_ENGINE_API AppConfig {
  std::string name{"Kuki Game"};
  std::filesystem::path logoPath;
  int screenWidth{1920};
  int screenHeight{1080};
  AppConfig(std::string, std::filesystem::path = "", int = 1920, int = 1080);
};
} // namespace kuki
