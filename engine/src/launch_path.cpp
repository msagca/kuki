#include <algorithm>
#include <filesystem>
#include <launch_path.hpp>
#include <string>
#include <system_error>
#include <vector>
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
auto AssetRoots() -> std::vector<std::filesystem::path> & {
  static std::vector<std::filesystem::path> roots;
  return roots;
}
} // namespace
auto GetLaunchPath() -> const std::filesystem::path & {
  static const std::filesystem::path path = ExecutableDirectory();
  return path;
}
auto ResolvePath(const std::filesystem::path &path) -> std::filesystem::path {
  if (path.empty() || path.is_absolute())
    return path;
  auto resolved = GetLaunchPath() / path;
  if (AssetRoots().empty())
    return resolved;
  std::error_code error;
  if (std::filesystem::exists(resolved, error))
    return resolved;
  for (const auto &root : AssetRoots())
    if (std::filesystem::exists(root / path, error))
      return root / path;
  return resolved;
}
auto AddAssetRoot(const std::filesystem::path &root) -> void {
  auto &roots = AssetRoots();
  if (std::ranges::find(roots, root) == roots.end())
    roots.push_back(root);
}
} // namespace kuki
