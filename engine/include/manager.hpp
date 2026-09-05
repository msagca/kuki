#pragma once
#include <kuki_engine_export.h>
namespace kuki {
class Application;
class KUKI_ENGINE_API Manager {
public:
  virtual ~Manager() = default;
protected:
  Manager(Application &);
  Application &app;
};
} // namespace kuki
