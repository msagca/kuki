#include <command.hpp>
#include <command_manager.hpp>
#include <iosfwd>
#include <span>
#include <sstream>
#include <string>
#include <vector>
namespace kuki {
ICommand::ICommand(const std::string name, Application& app)
  : name(name), app(app) {}
const std::string& ICommand::GetName() const {
  return name;
}
CommandManager::~CommandManager() {
  for (const auto& [_, command] : commands)
    delete command;
}
std::string CommandManager::GetCommands() {
  std::vector<std::string> commandList;
  for (const auto& [c, _] : commands)
    commandList.push_back(c);
  std::ostringstream result;
  result << commandList[0];
  for (auto i = 1; i < commandList.size(); ++i)
    result << ", " << commandList[i];
  return result.str();
}
std::vector<std::string> CommandManager::Tokenize(const std::string& input) {
  std::istringstream iss(input);
  std::vector<std::string> tokens;
  std::string token;
  while (iss >> token)
    tokens.push_back(token);
  return tokens;
}
void CommandManager::Register(ICommand* command) {
  auto& name = command->GetName();
  auto it = commands.find(name);
  if (it != commands.end())
    delete it->second; // NOTE: delete the existing pointer to prevent memory leaks
  commands[name] = command;
}
void CommandManager::Unregister(const std::string& name) {
  commands.erase(name);
}
int CommandManager::Dispatch(Application* app, const std::string& input, std::string& message) {
  auto tokens = Tokenize(input);
  if (tokens.empty()) {
    message = "No command provided.";
    return 1;
  }
  const auto& command = tokens.front();
  auto it = commands.find(command);
  if (it == commands.end()) {
    message = "Command not found: " + command + ", available commands: " + GetCommands();
    return 2;
  }
  std::span<std::string> args(tokens.begin() + 1, tokens.end());
  auto code = it->second->Execute(args);
  message = it->second->GetMessage(code);
  return code;
}
} // namespace kuki
