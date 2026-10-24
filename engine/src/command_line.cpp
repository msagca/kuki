#include <command_line.hpp>
#include <cstdlib>
#ifndef _WIN32
#include <fstream>
#endif
auto kuki::GetCommandLineArguments() -> std::vector<std::string> {
  std::vector<std::string> arguments;
#ifdef _WIN32
  // The C runtime keeps the process's arguments where any module in it can reach them, which is
  // what a library loaded by an executable that did not pass them on needs.
  for (auto index = 1; index < __argc; ++index)
    arguments.emplace_back(__argv[index]);
#else
  // Separated by nulls, the program's name first.
  std::ifstream file("/proc/self/cmdline", std::ios::binary);
  std::string argument;
  auto first = true;
  while (std::getline(file, argument, '\0')) {
    if (!first)
      arguments.push_back(argument);
    first = false;
  }
#endif
  return arguments;
}
