#pragma once
#include <cstdint>
#include <hash_utils.hpp>
namespace kuki {
enum class TargetFormat : uint8_t {
  R16,
  RGB16,
  RGB32,
  RGBA16,
  RGBA32
};
enum class TargetType : uint8_t {
  Cubemap,
  Texture2D,
  Texture2DMulti
};
struct TargetDescription {
  TargetFormat format{TargetFormat::RGBA16};
  TargetType target{TargetType::Texture2D};
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
    kuki::hash_combine(h, hash<int>{}(static_cast<int>(desc.target)));
    kuki::hash_combine(h, hash<int>{}(desc.width));
    kuki::hash_combine(h, hash<int>{}(desc.height));
    kuki::hash_combine(h, hash<int>{}(desc.samples));
    kuki::hash_combine(h, hash<int>{}(desc.mipmaps));
    return h;
  }
};
} // namespace std
