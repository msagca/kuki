#include <dx_format.hpp>
#ifdef KUKI_HAS_DIRECTX
#include <target_description.hpp>
namespace kuki {
auto IsDepthFormat(const TargetFormat &format) -> bool {
  return format == TargetFormat::DEPTH;
}
auto TargetFormatToDXGI(const TargetFormat &format) -> DXGI_FORMAT {
  switch (format) {
  case TargetFormat::R8:
    return DXGI_FORMAT_R8_UNORM;
  case TargetFormat::RG8:
    return DXGI_FORMAT_R8G8_UNORM;
  case TargetFormat::RGB8:
  case TargetFormat::RGBA8:
    return DXGI_FORMAT_R8G8B8A8_UNORM;
  case TargetFormat::SRGB8:
    return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
  case TargetFormat::R16:
    return DXGI_FORMAT_R16_FLOAT;
  case TargetFormat::RG16:
    return DXGI_FORMAT_R16G16_FLOAT;
  case TargetFormat::RGB16:
  case TargetFormat::RGBA16:
    return DXGI_FORMAT_R16G16B16A16_FLOAT;
  case TargetFormat::RGB32:
  case TargetFormat::RGBA32:
    return DXGI_FORMAT_R32G32B32A32_FLOAT;
  case TargetFormat::DEPTH:
    return DXGI_FORMAT_R32_TYPELESS;
  default:
    return DXGI_FORMAT_R8G8B8A8_UNORM;
  }
}
auto TargetFormatToSRVDXGI(const TargetFormat &format) -> DXGI_FORMAT {
  if (format == TargetFormat::DEPTH)
    return DXGI_FORMAT_R32_FLOAT;
  return TargetFormatToDXGI(format);
}
auto TargetFormatToRTVDXGI(const TargetFormat &format) -> DXGI_FORMAT {
  if (format == TargetFormat::DEPTH)
    return DXGI_FORMAT_D32_FLOAT;
  return TargetFormatToDXGI(format);
}
} // namespace kuki
#endif
