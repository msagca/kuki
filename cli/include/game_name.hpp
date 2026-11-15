#pragma once
#include <expected>
#include <string>
#include <string_view>
namespace kuki::cli {
/// @brief The forms a game's name takes in its repository, all from the one name `kuki new` is given.
///
/// `ddgi` is the directory `ddgi`, the namespace and file names `ddgi`, the class and target `Ddgi`
/// and the title `Ddgi`; `my-game` and `MyGame` are both `my_game` and `MyGame`, the directory
/// keeping what was typed. A word's capitals are kept, so `DDGI` stays `DDGI` in a class name.
struct GameName {
  /// As typed: the directory the repository is made in.
  std::string directory;
  /// `my_game`: the namespace, and the file names in `game/`.
  std::string snake;
  /// `MyGame`: the application class, the CMake project and the target.
  std::string pascal;
  /// `My Game`: the window's title.
  std::string title;
};
/// @brief Splits a name into words at `-`, `_` and the humps of a camel-cased one, and makes its forms.
///
/// Refuses what would not compile or would not do as a directory: anything but ASCII letters,
/// digits, `-` and `_`, a name not starting with a letter, and a namespace that is a C++ keyword or
/// one the engine already uses. The message says which.
auto ParseGameName(std::string_view) -> std::expected<GameName, std::string>;
/// @brief The template's text with its placeholder names -- `MyGame`, `my_game`, `My Game` -- made
/// the game's. Used on file contents and on file names alike.
auto RenameTemplate(std::string_view, const GameName &) -> std::string;
} // namespace kuki::cli
