#pragma once
#include <cstdint>
#include <material_fallback.hpp>
#include <material_type.hpp>
namespace kuki {
/// @brief Scene-side material state for the Direct3D 12 backend.
///
/// Deliberately free of Direct3D types so it can sit in `ComponentVariant` on every platform:
/// the engine core, the component reflection tables and the editor inspector all have to name
/// this type even in builds where Direct3D is not compiled in.
///
/// `textureTableGPU` is a GPU descriptor handle widened to `uint64_t` for the same reason. It
/// addresses a contiguous run of seven shader resource views, one per `TextureContent` slot in
/// declaration order, so the shader can index them positionally. Slots the material does not
/// supply hold a one-pixel white texture rather than nothing, which keeps the table complete;
/// `fallback.textureMask` is what tells the shader which slots are real.
///
/// Zero means the table was never built, and the shader falls back to the scalar material values.
struct DXMaterial {
  MaterialFallback fallback;
  MaterialType type{MaterialType::Unlit};
  uint64_t textureTableGPU{};
  uint32_t textureTableIndex{};
};
} // namespace kuki
