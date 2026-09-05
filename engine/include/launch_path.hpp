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
} // namespace kuki
