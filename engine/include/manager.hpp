#pragma once
#include <kuki_engine_export.h>
namespace kuki {
class Application;
class Manager {
public:
  virtual ~Manager() = default;
protected:
  Manager(Application &);
  Application &app;
};
} // namespace kuki
