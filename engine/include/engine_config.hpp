#pragma once
#include <filesystem>
#include <kuki_engine_export.h>
#include <post_process.hpp>
#include <rendering_api.hpp>
#include <string_view>
#include <tone_mapper.hpp>
namespace kuki {
/// @brief Display name of a backend, for menus and log lines.
auto KUKI_ENGINE_API ToString(const RenderingAPI) -> std::string_view;
/// @brief Whether a backend can actually be run here, which is asked of the driver, not the build.
///
/// Two things have to be true and only the first is known at compile time. The backend has to be
/// compiled in -- Direct3D 12 needs a Windows host with the SDK -- and the machine the executable
/// ends up on has to be able to run it. Those come apart the moment a build is copied anywhere: a
/// binary produced on a developer's machine may land on hardware with no feature level 11_0 device,
/// and a preprocessor test cannot see that.
///
/// So the Direct3D 12 answer creates a device and throws it away, once, and caches what happened.
/// Vulkan is false on the strength of the build alone: there is nothing to probe.
auto KUKI_ENGINE_API IsAvailable(const RenderingAPI) -> bool;
/// @brief The backend to use when nobody has said otherwise: the best one this machine can run.
///
/// Which is Direct3D 12 wherever it will run, and OpenGL everywhere else. The two are not equal --
/// only one of them traces the probe field, so only one renders the indirect bounce the default
/// scene was built to show -- and defaulting to the lesser of them on a machine that can run the
/// other means the first thing a new clone renders understates the engine.
///
/// Not a preference, which is the reason this is a function rather than a constant in the config
/// struct. A preference is what someone chose and belongs in `kuki.config.json`; this is what to do
/// when they have not chosen, and the right answer to that changes with the machine rather than
/// with the file. Someone who picks OpenGL on a Windows box has that saved and honoured.
auto KUKI_ENGINE_API DefaultAPI() -> RenderingAPI;
/// @brief Settings that belong to the installation rather than to any one scene.
///
/// Two kinds of thing end up here. Some cannot live in a scene at all: the graphics backend is
/// chosen at window creation and GLFW cannot change a window's client API afterwards, so it has to
/// be known before there is a scene to read it from. The rest could live in a scene but should not,
/// because they describe how someone wants to look at their work rather than what the work is. A
/// tone mapping curve travelling inside a scene file would follow it onto another machine and
/// override the preference there.
///
/// The exposure that feeds that curve is the other way round and lives on the camera, in the scene.
/// The split is deliberate: how much light the camera gathers is a fact about the shot, and how
/// that light is shown is a fact about the desk it is being shown on.
struct KUKI_ENGINE_API EngineConfig {
  RenderingAPI api{DefaultAPI()};
  ToneMapper toneMapper{DEFAULT_TONE_MAPPER};
  /// @brief Location of the config file, alongside the executable.
  static auto GetPath() -> std::filesystem::path;
  /// @brief Reads the config, falling back to defaults when the file is missing or malformed.
  static auto Load() -> EngineConfig;
  /// @brief Writes the config back to disk.
  /// @return False when the file could not be written.
  auto Save() const -> bool;
};
/// @brief The backend this installation has been set to, falling back to `DefaultAPI`.
///
/// What `ApplicationDescription::api` defaults to, so that every application honours the saved
/// preference without having to know it exists. The editor used to be the only thing that read it,
/// by writing `.api = EngineConfig::Load().api` into its own constructor -- which meant a game
/// built on the engine silently ignored the setting and always took `DefaultAPI`.
auto KUKI_ENGINE_API ConfiguredAPI() -> RenderingAPI;
} // namespace kuki
