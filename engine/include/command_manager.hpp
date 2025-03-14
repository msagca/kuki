#pragma once
#include <command.hpp>
#include <engine_export.h>
#include <unordered_map>
#include <vector>
class Application;
class ENGINE_API CommandManager {
private:
  std::unordered_map<std::string, ICommand*> commands;
  std::string GetCommands();
  std::vector<std::string> Tokenize(const std::string&);
public:
  ~CommandManager();
  void Register(ICommand*);
  void Unregister(const std::string&);
  int Dispatch(Application*, const std::string&, std::string&);
};
