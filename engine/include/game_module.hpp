#pragma once
#include <filesystem>
#include <kuki_engine_export.h>
#include <macros.hpp>
#include <string>
#include <vector>
namespace kuki {
class Application;
/// @brief What a game says about itself to whatever loads its library.
///
/// A game is a library: its systems and components, and one function that loads its assets and
/// builds its scenes into an application it is handed. Its own executable is a few lines that make
/// an application of the game's world and systems and call that function. The split was made so
/// that another application could load the game; nothing does now that the editor is gone.
///
/// So `setup` must not assume which application it is in. It is given one; it does not make one,
/// and anything it would have done in a constructor or a `Start` override belongs in it.
struct GameModule {
  const char *name;
  void (*setup)(Application &);
};
/// @brief A game's library, loaded into this process.
///
/// That needs a shared build: with the engine linked statically into each, a game would arrive
/// carrying a second engine of its own, with its own window system and its own log.
///
/// Never unloaded. Any scene the game built holds columns of the game's components, whose code
/// lives in the library, and taking it away from under them is a crash at whatever moment they are
/// next touched. A process has room for the games it was shown.
struct LoadedGame {
  const GameModule *module{};
  std::filesystem::path path;
};
/// @brief The symbol a game library exports, by which a host finds its `GameModule`.
inline constexpr const char *GAME_MODULE_SYMBOL = "KukiGameModule";
/// @brief Loads a game library and adds its directory to the places assets are looked for.
///
/// The second half matters as much as the first. A game names its assets relative to where it was
/// installed -- `model/fighter.glb` -- and resolves them against the executable's directory, which
/// is right when the executable is the game and wrong when it is another. See `AddAssetRoot`.
///
/// Empty `module` if the file is not a library, or is one that exports no module.
KUKI_ENGINE_API auto LoadGame(const std::filesystem::path &) -> LoadedGame;
/// @brief The game libraries found near the running executable: beside it, and in the directories
/// of a build tree's `game/`.
KUKI_ENGINE_API auto FindGames() -> std::vector<std::filesystem::path>;
} // namespace kuki
