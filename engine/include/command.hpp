#pragma once
#include <engine_export.h>
#include <span>
#include <string>
class Application;
class ENGINE_API ICommand {
protected:
  const std::string name;
  std::string message;
public:
  ICommand(const std::string);
  const std::string& GetName() const;
  virtual ~ICommand() = default;
  /// <summary>
  /// Get the message associated with the given return code
  /// </summary>
  /// <returns>The message string</returns>
  virtual std::string GetMessage(int = -1) = 0;
  virtual int Execute(Application*, const std::span<std::string>) = 0;
};
class ENGINE_API SpawnCommand : public ICommand {
public:
  SpawnCommand();
  std::string GetMessage(int) override;
  int Execute(Application*, const std::span<std::string>) override;
};
class ENGINE_API DeleteCommand : public ICommand {
public:
  DeleteCommand();
  std::string GetMessage(int) override;
  int Execute(Application*, const std::span<std::string>) override;
};
