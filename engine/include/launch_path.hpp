#pragma once
#include <filesystem>
#include <kuki_engine_export.h>
namespace kuki {
/// @brief Directory the running executable sits in, which is where the build stages its assets.
///
/// Everything an application needs at runtime is copied next to its binary, so this is the root
/// every relative asset path is written against. Resolved once on first use and kept, since the
/// executable cannot move while it is running.
auto KUKI_ENGINE_API GetLaunchPath() -> const std::filesystem::path &;
/// @brief Turns a path an asset was authored with into one that opens from any working directory.
///
/// A relative path in a scene, a manifest or a source file means "next to the executable", never
/// "next to wherever this process happened to be started". Tying them to the working directory
/// instead makes an application runnable only from its own staging directory, and fails in exactly
/// the way that looks like a missing asset rather than a wrong assumption about where to look.
///
/// Absolute paths are returned untouched: those come from a file dialog or a user, who has already
/// said where they mean.
auto KUKI_ENGINE_API ResolvePath(const std::filesystem::path &) -> std::filesystem::path;
/// @brief Adds a directory that relative paths are looked for in when the executable's has no such file.
///
/// For a game hosted by another application. The game says `model/fighter.glb` and means the file
/// beside itself, and when another application loaded it, beside itself is not beside the
/// executable. The executable's directory is still asked first, so nothing the host resolves
/// changes; a path found in neither is answered as it always was, against the executable.
auto KUKI_ENGINE_API AddAssetRoot(const std::filesystem::path &) -> void;
/// @brief The file an install puts beside the executables, which says they are installed.
///
/// Its presence is the whole message. The install rule writes it, and the build tree never has
/// it, so the same binary knows which of the two it is running from without being built twice.
inline constexpr auto INSTALLED_MARKER = "kuki.installed";
/// @brief Where an executable in the given directory writes what it keeps between runs.
///
/// The directory itself in a build tree, and the user's own data folder (`%LOCALAPPDATA%\Kuki` on
/// Windows) when `INSTALLED_MARKER` is beside it. An install for all users is in Program Files,
/// which a normal user cannot write to, so a game that wrote beside itself there lost its settings
/// on quitting and made its pipelines from scratch every launch. A build in the build tree goes on
/// writing beside itself: it is always writable, and keeping out of the user's folder means a
/// development build never shares a pipeline library, stamped with other shaders, or a player's
/// settings with an installed game.
///
/// Falls back to the directory when the user's folder cannot be found, so a lookup that fails
/// writes where it always did rather than nowhere.
auto KUKI_ENGINE_API GetUserDataPath(const std::filesystem::path &) -> std::filesystem::path;
/// @brief `GetUserDataPath` for the running executable, resolved once and kept.
///
/// The root of everything the engine writes: `kuki.config.json`, each game's preferences, and the
/// pipeline and texture caches under `cache/`. Reads of assets stay on `ResolvePath`, beside the
/// executable.
auto KUKI_ENGINE_API GetUserDataPath() -> const std::filesystem::path &;
} // namespace kuki
