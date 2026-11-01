#pragma once
#ifdef KUKI_HAS_DIRECTX
#include <dx_common.hpp>
#include <kuki_engine_export.h>
#include <target_description.hpp>
namespace kuki {
/// @brief Maps an engine target format onto the DXGI format used for the resource itself.
///
/// Depth targets return a typeless format, because the same resource has to be readable as a
/// shader resource and bindable as a depth-stencil, which DXGI only allows through a typeless
/// parent with typed views layered on top.
auto KUKI_ENGINE_API TargetFormatToDXGI(const TargetFormat &) -> DXGI_FORMAT;
/// @brief Typed format for a shader resource view onto a resource of this target format.
auto KUKI_ENGINE_API TargetFormatToSRVDXGI(const TargetFormat &) -> DXGI_FORMAT;
/// @brief Typed format for a render-target or depth-stencil view onto this target format.
auto KUKI_ENGINE_API TargetFormatToRTVDXGI(const TargetFormat &) -> DXGI_FORMAT;
/// @brief Whether the format is a depth format and so needs a DSV rather than an RTV.
auto KUKI_ENGINE_API IsDepthFormat(const TargetFormat &) -> bool;
} // namespace kuki
#endif
