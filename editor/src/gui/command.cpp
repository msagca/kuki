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
}
void Editor::ProcessSpawnCommand(const std::span<std::string> args) {
  if (args.size() < 2 || args.size() > 3) {
    std::cerr << "Invalid number of arguments to the 'spawn' command (expected 2 or 3, but received " << args.size() << ")." << std::endl;
    return;
  }
  auto assetName = args[0];
  auto assetID = GetAssetID(assetName);
  if (assetID < 0) {
    std::cerr << "An asset with the name '" << assetName << "' does not exist." << std::endl;
    return;
  }
  auto countStr = args[1];
  int count = 0;
  try {
    count = std::stoi(countStr);
  } catch (const std::exception& e) {
    std::cerr << "Invalid count: " << e.what() << std::endl;
    return;
  }
  float radius = 10.0f;
  if (args.size() > 2) {
    auto radiusStr = args[2];
    try {
      radius = std::stof(radiusStr);
    } catch (const std::exception& e) {
      std::cerr << "Invalid radius: " << e.what() << std::endl;
      return;
    }
  }
  SpawnMulti(assetName, count, radius);
}
void Editor::ProcessDeleteCommand(const std::span<std::string> args) {
  if (args.size() != 1) {
    std::cerr << "Invalid number of arguments to the 'delete' command (expected 1, but received " << args.size() << ")." << std::endl;
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
