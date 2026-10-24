#include <algorithm>
#include <command_line.hpp>
#include <cstdlib>
#include <format>
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
auto kuki::HasFlag(const std::span<const std::string> arguments, const Flag &flag) -> bool {
  return std::ranges::find(arguments, flag.name) != arguments.end();
}
auto kuki::GetFlagValue(const std::span<const std::string> arguments, const Flag &flag) -> std::optional<std::string> {
  const auto it = std::ranges::find(arguments, flag.name);
  if (it == arguments.end() || std::next(it) == arguments.end())
    return std::nullopt;
  return *std::next(it);
}
auto kuki::FindUnknownFlags(const std::span<const std::string> arguments, const std::span<const Flag> flags) -> std::vector<std::string> {
  std::vector<std::string> unknown;
  for (size_t index = 0; index < arguments.size(); ++index) {
    const auto &argument = arguments[index];
    const auto known = std::ranges::find(flags, std::string_view(argument), &Flag::name);
    if (known != flags.end()) {
      if (known->kind == FlagKind::Value)
        ++index;
      continue;
    }
    if (argument.starts_with("--"))
      unknown.push_back(argument);
  }
  return unknown;
}
auto kuki::DescribeFlags(const std::span<const Flag> flags) -> std::string {
  std::string text;
  for (const auto &flag : flags) {
    const auto usage = flag.value.empty() ? std::string(flag.name) : std::format("{} {}", flag.name, flag.value);
    text += std::format("  {:<24} {}\n", usage, flag.description);
  }
  return text;
}
