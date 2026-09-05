#pragma once
#include <id.hpp>
#include <kuki_engine_export.h>
namespace kuki {
class KUKI_ENGINE_API Shader {
public:
  virtual ~Shader() = default;
protected:
  Shader() = default;
};
} // namespace kuki
