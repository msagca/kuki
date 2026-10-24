#include <filesystem>
#include <game_module.hpp>
#include <launch_path.hpp>
#include <spdlog/spdlog.h>
#include <system_error>
#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif
namespace kuki {
namespace {
#if defined(_WIN32)
  constexpr const char *LIBRARY_EXTENSION = ".dll";
#elif defined(__APPLE__)
  constexpr const char *LIBRARY_EXTENSION = ".dylib";
#else
  constexpr const char *LIBRARY_EXTENSION = ".so";
#endif
  /// What a game library's name ends with before the extension. `kuki_add_game` is what names them.
  constexpr const char *LIBRARY_SUFFIX = "Game";
  auto IsGameLibrary(const std::filesystem::path &path) -> bool {
    return path.extension() == LIBRARY_EXTENSION && path.stem().string().ends_with(LIBRARY_SUFFIX);
  }
  auto Collect(const std::filesystem::path &directory, std::vector<std::filesystem::path> &found) -> void {
    std::error_code error;
    for (const auto &entry : std::filesystem::directory_iterator(directory, error))
      if (entry.is_regular_file(error) && IsGameLibrary(entry.path()))
        found.push_back(entry.path());
  }
} // namespace
auto LoadGame(const std::filesystem::path &path) -> LoadedGame {
  using Entry = const GameModule *(*)();
#if defined(_WIN32)
  // With the library's own directory searched for what it depends on, since that is where a game
  // keeps anything it brought with it. The engine is found already loaded.
  const auto handle = LoadLibraryExW(path.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
  const auto entry = handle ? reinterpret_cast<Entry>(reinterpret_cast<void *>(GetProcAddress(handle, GAME_MODULE_SYMBOL))) : nullptr;
#else
  const auto handle = dlopen(path.c_str(), RTLD_NOW | RTLD_GLOBAL);
  const auto entry = handle ? reinterpret_cast<Entry>(dlsym(handle, GAME_MODULE_SYMBOL)) : nullptr;
#endif
  if (!handle) {
    spdlog::error("[GameModule] {} could not be loaded", path.string());
    return {};
  }
  const auto *module = entry ? entry() : nullptr;
  if (!module || !module->setup) {
    spdlog::error("[GameModule] {} is a library but does not declare a game", path.string());
    return {};
  }
  AddAssetRoot(path.parent_path());
  spdlog::info("[GameModule] Loaded {} from {}", module->name, path.string());
  return {module, path};
}
auto FindGames() -> std::vector<std::filesystem::path> {
  std::vector<std::filesystem::path> found;
  const auto &here = GetLaunchPath();
  Collect(here, found);
  // In a build tree the editor is in `build/editor` and each game in `build/game/<name>`. In an
  // install they are all in one directory, and the line above has found them.
  std::error_code error;
  for (const auto &entry : std::filesystem::directory_iterator(here.parent_path() / "game", error))
    if (entry.is_directory(error))
      Collect(entry.path(), found);
  return found;
}
} // namespace kuki
