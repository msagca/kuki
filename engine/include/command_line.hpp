#pragma once
#include <kuki_engine_export.h>
#include <string>
#include <vector>
namespace kuki {
/// @brief The arguments the process was started with, the program's own name left out.
///
/// For a game, whose `main` is a few lines in `cmake/game_main.cpp` shared by all of them and so has
/// no arguments to pass on. Empty if the platform will not say.
KUKI_ENGINE_API auto GetCommandLineArguments() -> std::vector<std::string>;
} // namespace kuki
