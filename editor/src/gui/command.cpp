#include <editor.hpp>
#include <iostream>
#include <span>
#include <sstream>
#include <vector>
std::vector<std::string> Editor::Tokenize(const std::string& input) {
  std::istringstream iss(input);
  std::vector<std::string> tokens;
  std::string token;
  while (iss >> token)
    tokens.push_back(token);
  return tokens;
}
void Editor::ProcessCommand(const std::string& input) {
  auto tokens = Tokenize(input);
  if (tokens.empty())
    return;
  const auto& command = tokens.front();
  std::span<std::string> args(tokens.begin() + 1, tokens.end());
  if (command == "spawn")
    ProcessSpawnCommand(args);
  else if (command == "delete" || command == "del")
    ProcessDeleteCommand(args);
  else {
    consoleMessage = "Unknown command: " + command;
    hasConsoleMessage = true;
  }
}
void Editor::ProcessSpawnCommand(const std::span<std::string> args) {
  if (args.size() < 1 || args.size() > 3) {
    consoleMessage = "Usage: spawn <item-name> [item-count] [spawn-radius]";
    hasConsoleMessage = true;
    return;
  }
  auto assetName = args[0];
  auto assetID = GetAssetID(assetName);
  if (assetID < 0) {
    consoleMessage = "Asset does not exist:" + assetName;
    hasConsoleMessage = true;
    return;
  }
  int count = 1;
  if (args.size() > 1) {
    auto countStr = args[1];
    try {
      count = std::stoi(countStr);
    } catch (const std::exception& e) {
      consoleMessage = "Invalid item count: " + countStr;
      hasConsoleMessage = true;
      return;
    }
  }
  float radius = 10.0f;
  if (args.size() > 2) {
    auto radiusStr = args[2];
    try {
      radius = std::stof(radiusStr);
    } catch (const std::exception& e) {
      consoleMessage = "Invalid spawn radius: " + radiusStr;
      hasConsoleMessage = true;
      return;
    }
  }
  SpawnMulti(assetName, count, radius);
}
void Editor::ProcessDeleteCommand(const std::span<std::string> args) {
  if (args.size() != 1) {
    consoleMessage = "Usage: delete/del <name-pattern>";
    hasConsoleMessage = true;
    return;
  }
  auto pattern = args[0];
  auto last = pattern.back();
  if (last == '*') {
    pattern.pop_back();
    DeleteAllEntities(pattern);
  } else
    DeleteEntity(pattern);
}
