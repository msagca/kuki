// The whole of every game's executable. The game itself is a library -- see `kuki_add_game` in the
// root CMakeLists.txt and `game_module.hpp` -- and this is the few lines that start it when it is
// run on its own rather than inside the editor. `KUKI_GAME_MAIN` is what defines the function.
extern "C" auto KukiGameRun() -> int;
auto main() -> int {
  return KukiGameRun();
}
