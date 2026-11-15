#pragma once
#include <filesystem>
#include <string>
#include <vector>
namespace kuki::cli {
/// @brief Runs a program found on the PATH, in a directory, with its output on this console, and
/// waits for it.
///
/// The arguments are handed over as they are, one each, rather than through a shell: a path with a
/// space in it, which a home directory on Windows often has, needs no quoting by the caller.
///
/// @return The program's exit code, or -1 when it could not be started -- not on the PATH, most
/// likely, which is said on the console.
auto Run(const std::vector<std::string> &arguments, const std::filesystem::path &directory = {}) -> int;
/// @brief Whether a program can be started at all, asked by running it with `--version` quietly.
auto IsAvailable(const std::vector<std::string> &arguments) -> bool;
} // namespace kuki::cli
