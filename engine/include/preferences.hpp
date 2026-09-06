#pragma once
#include <filesystem>
#include <kuki_engine_export.h>
#include <string>
#include <string_view>
#include <unordered_map>
namespace kuki {
/// @brief Small named values an application wants to find again next time it runs.
///
/// The counterpart of `EngineConfig` for things the engine has no opinion about: that file holds
/// the settings the engine itself reads, and this holds whatever a game decides to keep. A time
/// control, a difficulty, which level was reached. The engine never looks inside.
///
/// Deliberately not components in a saved scene, which is the other place this could have gone.
/// A scene records what is in a world; a preference records what someone chose about the game,
/// which outlives any particular world and belongs to nothing in it. Chess builds its scene in
/// code every launch and has no scene file at all, so putting two integers in one would have
/// meant inventing a save and a load for the sake of the two integers -- and `ComponentType` is a
/// closed enum and `ComponentVariant` a closed variant, so a game cannot declare a component of
/// its own to put them in without the engine being changed to know about that game.
///
/// Everything is stored as text and converted on the way out, so a value written by one version
/// and read by another that has changed its mind about the type reads as the default rather than
/// as nonsense. Absent and unparseable are the same answer for the same reason: a caller has to
/// have a sensible default anyway, and a preferences file is the last thing that should be able to
/// stop a game starting.
class KUKI_ENGINE_API Preferences {
public:
  /// @brief Where an application's preferences live: beside the executable, named after it.
  ///
  /// Named after the application so that two games installed side by side keep their own, and
  /// beside the executable so it travels with the build exactly as `kuki.config.json` does.
  static auto GetPath(const std::string &) -> std::filesystem::path;
  /// @brief Reads the file, leaving this empty when it is missing or malformed.
  auto Load(const std::string &) -> bool;
  /// @brief Writes the file. Does nothing and succeeds when nothing has been set.
  auto Save() const -> bool;
  auto Has(const std::string_view) const -> bool;
  auto GetInt(const std::string_view, const int = 0) const -> int;
  auto GetFloat(const std::string_view, const float = .0f) const -> float;
  auto GetBool(const std::string_view, const bool = false) const -> bool;
  auto GetString(const std::string_view, const std::string & = {}) const -> std::string;
  auto Set(std::string, const int) -> void;
  auto Set(std::string, const float) -> void;
  auto Set(std::string, const bool) -> void;
  auto Set(std::string, std::string) -> void;
  auto Remove(const std::string_view) -> bool;
  auto Clear() -> void;
  auto IsEmpty() const -> bool;
private:
  auto Find(const std::string_view) const -> const std::string *;
  std::unordered_map<std::string, std::string> values;
  std::filesystem::path path;
};
} // namespace kuki
