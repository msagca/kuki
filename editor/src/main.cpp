#include <editor.hpp>
#include <string_view>
/// `--game <name>` opens a game as the editor starts, by any part of its library's name, and
/// `--play` starts it playing. Together they are how a game is launched in the editor from a
/// shortcut or an IDE's run button, without a visit to the play bar first.
auto main(int argc, char **argv) -> int {
  Editor editor;
  for (auto i = 1; i < argc; ++i) {
    const std::string_view argument = argv[i];
    if (argument == "--game" && i + 1 < argc)
      editor.startGame = argv[++i];
    else if (argument == "--play")
      editor.startPlaying = true;
  }
  editor.Run();
  return 0;
}
