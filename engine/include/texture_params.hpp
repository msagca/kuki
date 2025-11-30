#pragma once
#include <cstddef>
#include <functional>
#include <gl_constants.hpp>
#include <kuki_engine_export.h>
namespace kuki {
struct KUKI_ENGINE_API TextureParams {
  int width{1024};
  int height{1024};
  int target{GLConst::TEXTURE_2D};
  int format{GLConst::RGB16F};
  int samples{1};
  int mipmaps{1};
  TextureParams(int = 1024, int = 1024, int = GLConst::TEXTURE_2D, int = GLConst::RGB16F, int = 1, int = 1);
  bool operator==(const TextureParams&) const;
};
} // namespace kuki
namespace std {
template <>
struct hash<kuki::TextureParams> {
  size_t operator()(const kuki::TextureParams& params) const noexcept {
    auto hasher = std::hash<int>{};
    auto h = hasher(params.width);
    h ^= hasher(params.height) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= hasher(params.target) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= hasher(params.format) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= hasher(params.samples) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= hasher(params.mipmaps) + 0x9e3779b9 + (h << 6) + (h >> 2);
    return h;
  }
};
} // namespace std
