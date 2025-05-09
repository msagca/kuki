#pragma once
#include <kuki_engine_export.h>
#include <span>
#include <string>
namespace kuki {
class Application;
class KUKI_ENGINE_API ICommand {
protected:
  const std::string name;
  std::string message;
  Application& app;
public:
  ICommand(const std::string, Application&);
  const std::string& GetName() const;
  virtual ~ICommand() = default;
  /// @brief Get the message associated with the given return code
  /// @return The message string
  virtual std::string GetMessage(int = -1) = 0;
  virtual int Execute(const std::span<std::string>) = 0;
};
class KUKI_ENGINE_API SpawnCommand final : public ICommand {
public:
  SpawnCommand(Application&);
  std::string GetMessage(int) override;
  int Execute(const std::span<std::string>) override;
};
class KUKI_ENGINE_API DeleteCommand final : public ICommand {
public:
  DeleteCommand(Application&);
  std::string GetMessage(int) override;
  int Execute(const std::span<std::string>) override;
};
} // namespace kuki
