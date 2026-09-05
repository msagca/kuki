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
  DEPTH,
  Unknown
};
enum class TargetType : uint8_t {
  Cubemap,
  Texture2D,
  Texture2DArray,
  Texture2DMulti
};
/// @brief Whether a render target's size follows the viewport, half of it, or stays as declared.
///
/// Most targets in a graph are viewport-sized, but some deliberately are not: a shadow map is
/// declared oversized for texel density and must keep that size when the viewport changes, or its
/// resolution silently collapses to whatever the editor panel happens to be.
///
/// `ViewportHalf` tracks the viewport at half its width and height, so a quarter of the pixels. It
/// exists for the bloom chain, whose every pass is a blur: detail there is destroyed by design, so
/// resolving it at full size would be paying four times over for texels that get averaged away.
enum class TargetSizing : uint8_t {
  Viewport,
  ViewportHalf,
  Fixed
};
struct TargetDescription {
  TargetFormat format{TargetFormat::RGBA16};
  TargetType type{TargetType::Texture2D};
  int width{1024};
  int height{1024};
  int samples{1};
  int mipmaps{1};
  int layers{1};
  bool pickingBuffer{false};
  auto operator==(const TargetDescription &) const noexcept -> bool = default;
};
} // namespace kuki
namespace std {
template <>
struct hash<kuki::TargetDescription> {
  auto operator()(const kuki::TargetDescription &desc) const noexcept -> size_t {
    size_t h{};
    kuki::hash_combine(h, hash<int>{}(static_cast<int>(desc.format)));
    kuki::hash_combine(h, hash<int>{}(static_cast<int>(desc.type)));
    kuki::hash_combine(h, hash<int>{}(desc.width));
    kuki::hash_combine(h, hash<int>{}(desc.height));
    kuki::hash_combine(h, hash<int>{}(desc.samples));
    kuki::hash_combine(h, hash<int>{}(desc.mipmaps));
    kuki::hash_combine(h, hash<int>{}(desc.layers));
    kuki::hash_combine(h, hash<bool>{}(desc.pickingBuffer));
    return h;
  }
};
} // namespace std
