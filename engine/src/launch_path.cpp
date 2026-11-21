#include <algorithm>
#include <filesystem>
#include <launch_path.hpp>
#include <string>
#include <system_error>
#include <vector>
#include <whereami.h>
#ifdef _WIN32
#include <shlobj.h>
#else
#include <cstdlib>
#endif
namespace kuki {
namespace {
  /// @brief The folder the platform gives a user for an application's data, or empty if none is found.
  ///
  /// The local folder on Windows rather than the roaming one: the caches are large and tied to the
  /// GPU and its driver, and the graphics settings to the machine, so none of it should follow a
  /// user to another computer. Asked of the shell rather than read from `LOCALAPPDATA`, which is only
  /// a copy of the answer and can be missing from a process's environment or out of date in it.
  auto PlatformDataDirectory() -> std::filesystem::path {
#ifdef _WIN32
    PWSTR folder = nullptr;
    std::filesystem::path path;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr, &folder)))
      path = folder;
    CoTaskMemFree(folder);
    return path;
#elif defined(__APPLE__)
    const auto *home = std::getenv("HOME");
    return home && *home ? std::filesystem::path(home) / "Library" / "Application Support" : std::filesystem::path{};
#else
    if (const auto *data = std::getenv("XDG_DATA_HOME"); data && *data)
      return data;
    const auto *home = std::getenv("HOME");
    return home && *home ? std::filesystem::path(home) / ".local" / "share" : std::filesystem::path{};
#endif
  }
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
auto GetUserDataPath(const std::filesystem::path &directory) -> std::filesystem::path {
  std::error_code error;
  if (!std::filesystem::exists(directory / INSTALLED_MARKER, error))
    return directory;
  const auto data = PlatformDataDirectory();
  // One folder for every game rather than one each: the engine's config is shared by all of them
  // (one installation, one set of graphics settings), preferences and pipeline libraries are
  // already named after their game, and a cached texture is named after its source's absolute
  // path, so nothing one game writes there can be mistaken for another's.
  return data.empty() ? directory : data / "Kuki";
}
auto GetUserDataPath() -> const std::filesystem::path & {
  static const std::filesystem::path path = GetUserDataPath(GetLaunchPath());
  return path;
}
} // namespace kuki
