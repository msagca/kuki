#pragma once
#include <filesystem>
#include <kuki_engine_export.h>
#include <string>
#include <vector>
#if defined(_WIN32)
#define KUKI_MODULE_EXPORT __declspec(dllexport)
#else
#define KUKI_MODULE_EXPORT __attribute__((visibility("default")))
#endif
namespace kuki {
class Application;
/// @brief What a game says about itself to whatever is hosting it.
///
/// A game is a library: its scripts, and one function that loads its assets and builds its scenes
/// into an application it is handed. Its own executable is a few lines that make an application and
/// call that function. The editor is another application that can call it, which is the whole of
/// how a game comes to run inside the editor -- the same code, built once, set up by a different
/// host.
///
/// So `setup` must not assume which application it is in. It is given one; it does not make one,
/// and anything it would have done in a constructor or a `Start` override belongs in it.
struct GameModule {
  const char *name;
  void (*setup)(Application &);
};
/// @brief A game's library, loaded into this process.
///
/// Loading runs the library's static initialisers, which is where its `KUKI_REGISTER_SCRIPT` lines
/// put its scripts in the registry -- the one registry, since the engine is itself a shared library
/// that host and game both link. That is why this needs a shared build: with the engine linked
/// statically into each, a game would arrive carrying a second engine of its own, with its own
/// registry, its own window system and its own log.
///
/// Never unloaded. The registry and any scene the game built hold functions and objects whose code
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
/// is right when the executable is the game and wrong when it is the editor. See `AddAssetRoot`.
///
/// Empty `module` if the file is not a library, or is one that exports no module.
KUKI_ENGINE_API auto LoadGame(const std::filesystem::path &) -> LoadedGame;
/// @brief The game libraries found near the running executable: beside it, and in the directories
/// of a build tree's `game/`.
KUKI_ENGINE_API auto FindGames() -> std::vector<std::filesystem::path>;
} // namespace kuki
/// @brief Declares a library to be a game. Once, in one source file of the game.
#define KUKI_GAME_MODULE(Name, Setup) \
  extern "C" KUKI_MODULE_EXPORT auto KukiGameModule() -> const ::kuki::GameModule * { \
    static const ::kuki::GameModule module{Name, Setup}; \
    return &module; \
  }
/// @brief Gives a game's own executable something to call, so its `main` needs no header of the game's.
///
/// `App` is the game's application class. It stays inside the library, where nothing has to be
/// exported for it; the executable sees one C function.
#define KUKI_GAME_MAIN(App) \
  extern "C" KUKI_MODULE_EXPORT auto KukiGameRun() -> int { \
    App app; \
    app.Run(); \
    return 0; \
  }
