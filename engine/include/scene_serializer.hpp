#pragma once
#include <filesystem>
#include <kuki_engine_export.h>
namespace kuki {
class Application;
class KUKI_ENGINE_API SceneSerializer {
public:
  static auto Save(Application &, const std::filesystem::path &) -> bool;
  static auto Load(Application &, const std::filesystem::path &) -> bool;
};
} // namespace kuki
