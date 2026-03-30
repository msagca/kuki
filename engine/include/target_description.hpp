#pragma once
#include <cstdint>
#include <functional>
#include <hash_utils.hpp>
namespace kuki {
enum class TargetFormat : uint8_t {
  R8,
  RG8,
  RGB8,
  R16,
  RG16,
  RGB16,
  RGB32,
  RGBA8,
  RGBA16,
  RGBA32,
  SRGB8,
  Unknown
};
struct TargetDescription {
  // TODO: add a type member to identify cubemaps
  TargetFormat format{TargetFormat::RGBA16};
  int width{1024};
  int height{1024};
  int samples{1};
  int mipmaps{1};
  auto operator==(const TargetDescription &) const noexcept -> bool = default;
};
} // namespace kuki
namespace std {
template <>
struct hash<kuki::TargetDescription> {
  auto operator()(const kuki::TargetDescription &desc) const noexcept -> size_t {
    size_t h{};
    kuki::hash_combine(h, hash<int>{}(static_cast<int>(desc.format)));
    kuki::hash_combine(h, hash<int>{}(desc.width));
    kuki::hash_combine(h, hash<int>{}(desc.height));
    kuki::hash_combine(h, hash<int>{}(desc.samples));
    kuki::hash_combine(h, hash<int>{}(desc.mipmaps));
    return h;
  }
};
} // namespace std
