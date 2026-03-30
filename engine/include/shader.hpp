#pragma once
#include <id.hpp>
namespace kuki {
class Shader {
public:
  virtual ~Shader() = default;
protected:
  Shader() = default;
};
} // namespace kuki
