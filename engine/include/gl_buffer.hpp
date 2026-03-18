#pragma once
#include <buffer_object.hpp>
#include <target_description.hpp>
namespace kuki {
struct GLBuffer final : public BufferObject {
  unsigned int id{};
  explicit operator bool() const;
};
} // namespace kuki
