#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <kuki_engine_export.h>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
namespace kuki {
/// @brief The arguments the process was started with, the program's own name left out.
///
/// For a game, whose `main` is a few lines in `cmake/game_main.cpp` shared by all of them and so has
/// no arguments to pass on. Empty if the platform will not say.
KUKI_ENGINE_API auto GetCommandLineArguments() -> std::vector<std::string>;
enum class FlagKind : uint8_t {
  /// Given or not, with nothing after it.
  Switch,
  /// Followed by one value.
  Value
};
/// @brief A command-line flag, declared by whatever reads it.
///
/// A system that reads the command line lists its flags as `static constexpr std::array FLAGS`,
/// and `SystemApplication` puts every listed system's together with the editor's and checks at
/// compile time that no two share a name. So two parts of one application cannot quietly read the
/// same flag as two different things, and a game cannot take one of the editor's: that is a build
/// that fails, not a flag that does two things at once.
///
/// The check covers what is compiled together. A game loaded into the editor brings its systems'
/// flags with it, which the editor never sees, and those are checked against the editor's in the
/// game's own build instead -- every application's check includes the editor's flags.
struct Flag {
  /// With its dashes: `--host`.
  std::string_view name;
  FlagKind kind{FlagKind::Switch};
  /// What follows a `Value` flag, as `--help` writes it: `<port>`.
  std::string_view value;
  std::string_view description;
};
/// @brief Whether no two of the flags have the same name.
constexpr auto HasDistinctNames(const std::span<const Flag> flags) -> bool {
  for (size_t i = 0; i < flags.size(); ++i)
    for (size_t j = i + 1; j < flags.size(); ++j)
      if (flags[i].name == flags[j].name)
        return false;
  return true;
}
/// @brief One list of the flags of several.
template <size_t... Ns>
constexpr auto JoinFlags(const std::array<Flag, Ns> &...lists) -> std::array<Flag, (Ns + ... + 0)> {
  std::array<Flag, (Ns + ... + 0)> joined{};
  size_t next = 0;
  ((
     [&] {
       for (const auto &flag : lists)
         joined[next++] = flag;
     }()),
    ...);
  return joined;
}
template <typename T>
concept HasFlags = requires {
  { std::span<const Flag>(T::FLAGS) };
};
/// @brief A type's flags, or none for one that declares no `FLAGS`.
template <typename T>
constexpr auto FlagsOf() {
  if constexpr (HasFlags<T>)
    return T::FLAGS;
  else
    return std::array<Flag, 0>{};
}
/// @brief What the editor reads, which every application's check includes so that no game can take them.
inline constexpr Flag GAME_FLAG{"--game", FlagKind::Value, "<name>", "open the game whose library's name has this in it (the editor)"};
inline constexpr Flag PLAY_FLAG{"--play", FlagKind::Switch, "", "start the game it opened playing (the editor)"};
/// @brief Lists every flag the application knows and does nothing else.
inline constexpr Flag HELP_FLAG{"--help", FlagKind::Switch, "", "list these and stop"};
/// @brief Saves one finished frame as a picture and closes, for a look at what a build draws without
/// anyone at the screen: a check run from a script, or a picture for a change's description.
inline constexpr Flag CAPTURE_FLAG{"--capture", FlagKind::Value, "<file.png>", "save the frame drawn after --capture-frame frames to this picture and quit (Direct3D 12)"};
inline constexpr Flag CAPTURE_FRAME_FLAG{"--capture-frame", FlagKind::Value, "<count>", "how many frames to draw before --capture takes one (300)"};
inline constexpr std::array ENGINE_FLAGS{GAME_FLAG, PLAY_FLAG, HELP_FLAG, CAPTURE_FLAG, CAPTURE_FRAME_FLAG};
/// @brief Whether a flag was given.
KUKI_ENGINE_API auto HasFlag(const std::span<const std::string>, const Flag &) -> bool;
/// @brief What follows a flag, if it was given with something after it.
KUKI_ENGINE_API auto GetFlagValue(const std::span<const std::string>, const Flag &) -> std::optional<std::string>;
/// @brief The arguments that look like flags and are none of these. A flag's value is not counted,
/// so `--host 47010` is one known flag and not a flag and a stranger.
KUKI_ENGINE_API auto FindUnknownFlags(const std::span<const std::string>, const std::span<const Flag>) -> std::vector<std::string>;
/// @brief The flags as `--help` writes them, one to a line.
KUKI_ENGINE_API auto DescribeFlags(const std::span<const Flag>) -> std::string;
} // namespace kuki
