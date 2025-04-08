#pragma once
#include <kuki_export.h>
#include <span>
#include <string>
class Application;
class KUKI_API ICommand {
protected:
  const std::string name;
  std::string message;
public:
  ICommand(const std::string);
  const std::string& GetName() const;
  virtual ~ICommand() = default;
  /// @brief Get the message associated with the given return code
  /// @return The message string
  virtual std::string GetMessage(int = -1) = 0;
  virtual int Execute(Application*, const std::span<std::string>) = 0;
};
class KUKI_API SpawnCommand final : public ICommand {
public:
  SpawnCommand();
  std::string GetMessage(int) override;
  int Execute(Application*, const std::span<std::string>) override;
};
class KUKI_API DeleteCommand final : public ICommand {
public:
  DeleteCommand();
  std::string GetMessage(int) override;
  int Execute(Application*, const std::span<std::string>) override;
};
