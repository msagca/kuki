#pragma once
#include <cstddef>
#include <functional>
namespace kuki {
struct BufferDescription {
  size_t size{64};
  auto operator==(const BufferDescription &) const -> bool;
};
} // namespace kuki
namespace std {
template <>
struct hash<kuki::BufferDescription> {
  auto operator()(const kuki::BufferDescription &desc) const noexcept -> size_t {
    return hash<size_t>{}(desc.size);
  }
};
} // namespace std
