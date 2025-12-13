#include <command.hpp>
#include <editor.hpp>
#include <span>
#include <spdlog/spdlog.h>
#include <string>
using namespace kuki;
DeleteCommand::DeleteCommand(Editor& app)
  : ICommand("delete"), app(app) {}
std::string DeleteCommand::GetMessage(int code) {
  switch (code) {
  case 0:
    return "";
  default:
    return "Usage: delete <name_pattern>";
  }
}
int DeleteCommand::Execute(const std::span<std::string> args) {
  if (args.size() != 1)
    return -1;
  auto& pattern = args[0];
  spdlog::info("Deleted entities whose names matched the pattern '{}'.", pattern);
  auto last = pattern.back();
  if (last == '*') {
    pattern.pop_back();
    app.DeleteAllEntities(pattern);
  } else
    app.DeleteEntity(pattern);
  message = "";
  return 0;
}
