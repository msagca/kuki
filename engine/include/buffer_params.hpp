#pragma once
#include <gl_constants.hpp>
#include <kuki_engine_export.h>
#include <pool.hpp>
namespace kuki {
struct KUKI_ENGINE_API BufferParams {
  GLConst::SIZEIPTR size;
  BufferParams(GLConst::SIZEIPTR);
  bool operator==(const BufferParams&) const;
};
} // namespace kuki
namespace std {
template <>
struct hash<kuki::BufferParams> {
  size_t operator()(const kuki::BufferParams& params) const noexcept {
    auto hasher = std::hash<int>{};
    return hasher(params.size);
  }
};
} // namespace std
