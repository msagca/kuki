#pragma once
#include <buffer_object.hpp>
#include <kuki_engine_export.h>
#include <target_description.hpp>
namespace kuki {
struct KUKI_ENGINE_API GLBuffer final : public BufferObject {
  unsigned int id{};
  explicit operator bool() const;
};
} // namespace kuki
