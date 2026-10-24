#include <command_line.hpp>
#include <editor.hpp>
#include <spdlog/spdlog.h>
/// `--play` starts the scene playing as the editor opens. `--game` used to open a game's library
/// first, and no longer can: see the note on `Editor`. It is still one of the engine's flags, so
/// that no game can take the name, and is answered here rather than ignored.
auto main() -> int {
  Editor editor;
  const auto arguments = kuki::GetCommandLineArguments();
  if (kuki::HasFlag(arguments, kuki::GAME_FLAG))
    spdlog::warn("[Editor] {} was given, but the editor cannot host a game while a game's behaviour is its own systems; opening the editor's scene instead", kuki::GAME_FLAG.name);
  editor.startPlaying = kuki::HasFlag(arguments, kuki::PLAY_FLAG);
  editor.Run();
  return 0;
}
