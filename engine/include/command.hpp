#pragma once
#include <kuki_engine_export.h>
#include <span>
#include <string>
namespace kuki {
class KUKI_ENGINE_API ICommand {
protected:
  const std::string name;
  std::string message;
public:
  ICommand(const std::string&);
  const std::string& GetName() const;
  virtual ~ICommand() = default;
  /// @brief Get the message associated with the given return code
  /// @return The message string
  virtual std::string GetMessage(int = -1) = 0;
  virtual int Execute(const std::span<std::string>) = 0;
};
} // namespace kuki
