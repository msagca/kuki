#include <command.hpp>
#include <editor.hpp>
#include <exception>
#include <span>
#include <spdlog/spdlog.h>
#include <string>
using namespace kuki;
SpawnCommand::SpawnCommand(Editor& app)
  : ICommand("spawn"), app(app) {}
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
int SpawnCommand::Execute(const std::span<std::string> args) {
  if (args.size() < 1 || args.size() > 3)
    return -1;
  auto& assetName = args[0];
  auto assetId = app.GetAssetId(assetName);
  if (!assetId.IsValid()) {
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
  app.InstantiateRandom(assetName, count, radius);
  spdlog::info("Created {} copies of the asset '{}' within a radius of {} units.", count, assetName, radius);
  message = "";
  return 0;
}
