#pragma once
#include <target_description.hpp>
namespace kuki {
class RenderTarget {
public:
  virtual ~RenderTarget() = default;
  TargetDescription desc;
protected:
  RenderTarget() = default;
};
} // namespace kuki
