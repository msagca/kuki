#pragma once
#include <cstdint>
#include <kuki_engine_export.h>
#include <target_description.hpp>
namespace kuki {
/// @brief Base for anything a render pass can draw into and a later pass can read.
class KUKI_ENGINE_API RenderTarget {
public:
  virtual ~RenderTarget() = default;
  TargetDescription desc;
protected:
  RenderTarget() = default;
};
} // namespace kuki
