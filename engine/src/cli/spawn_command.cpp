#include <application.hpp>
#include <command.hpp>
#include <exception>
#include <span>
#include <string>
SpawnCommand::SpawnCommand()
  : ICommand("spawn") {}
std::string SpawnCommand::GetMessage(int code) {
  switch (code) {
  case 0:
    return "";
  case -2:
    return "Asset does not exist: " + message;
  case -3:
    return "Invalid item count: " + message;
  case -4:
    return "Invalid spawn radius: " + message;
  default:
    return "Usage: spawn <item_name> [ <item_count> [spawn_radius] ]";
  }
}
int SpawnCommand::Execute(Application* app, const std::span<std::string> args) {
  if (args.size() < 1 || args.size() > 3)
    return -1;
  auto& assetName = args[0];
  auto assetID = app->GetAssetID(assetName);
  if (assetID < 0) {
    message = assetName;
    return -2;
  }
  int count = 1;
  if (args.size() > 1) {
    auto& countStr = args[1];
    try {
      count = std::stoi(countStr);
    } catch (const std::exception& e) {
      message = countStr;
      return -3;
    }
  }
  float radius = 10.0f;
  if (args.size() > 2) {
    auto& radiusStr = args[2];
    try {
      radius = std::stof(radiusStr);
    } catch (const std::exception& e) {
      message = radiusStr;
      return -4;
    }
  }
  app->SpawnMulti(assetName, count, radius);
  message = "";
  return 0;
}
