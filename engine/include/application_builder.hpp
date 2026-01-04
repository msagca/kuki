#pragma once
#include <application.hpp>
#include <kuki_engine_export.h>
#include <memory>
namespace kuki {
class KUKI_ENGINE_API ApplicationBuilder {
public:
  auto Build() -> std::unique_ptr<Application>;
private:
  std::unique_ptr<Application> app;
};
} // namespace kuki
