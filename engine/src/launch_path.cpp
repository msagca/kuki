#include <filesystem>
#include <launch_path.hpp>
#include <string>
#include <whereami.h>
namespace kuki {
namespace {
auto ExecutableDirectory() -> std::filesystem::path {
  const auto length = wai_getExecutablePath(nullptr, 0, nullptr);
  if (length <= 0)
    return std::filesystem::current_path();
  std::string path(length, '\0');
  wai_getExecutablePath(path.data(), length, nullptr);
  return std::filesystem::path(path).parent_path();
}
} // namespace
auto GetLaunchPath() -> const std::filesystem::path & {
  static const std::filesystem::path path = ExecutableDirectory();
  return path;
}
auto ResolvePath(const std::filesystem::path &path) -> std::filesystem::path {
  if (path.empty() || path.is_absolute())
    return path;
  return GetLaunchPath() / path;
}
} // namespace kuki
