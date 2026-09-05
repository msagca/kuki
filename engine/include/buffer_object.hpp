#pragma once
#include <kuki_engine_export.h>
namespace kuki {
class KUKI_ENGINE_API BufferObject {
public:
  virtual ~BufferObject() = default;
protected:
  BufferObject() = default;
};
} // namespace kuki
