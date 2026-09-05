#pragma once
#include <hash_utils.hpp>
#include <kuki_engine_export.h>
#include <material_fallback.hpp>
#include <material_type.hpp>
namespace kuki {
struct GLTextures {
  unsigned int albedo{};
  unsigned int normal{};
  unsigned int metalness{};
  unsigned int occlusion{};
  unsigned int roughness{};
  unsigned int specular{};
  unsigned int emissive{};
};
class GLShader;
/// @brief A material as the OpenGL backend holds it: texture names plus the fallback values.
///
/// Equality and the hash below decide what shares a render bucket, and a bucket is drawn with one
/// `Apply`. So they compare exactly the fields `Apply` uploads as uniforms, and deliberately not
/// the ones that reach the shader per instance: two entities differing only in albedo belong in
/// the same bucket, two differing in alpha mode or index of refraction do not, because only the
/// first material's values would ever be set.
struct KUKI_ENGINE_API GLMaterial {
  GLTextures textures{};
  MaterialFallback fallback{};
  MaterialType type{MaterialType::Unlit};
  auto Apply(const GLShader &) const -> void;
  auto operator==(const GLMaterial &) const -> bool;
};
} // namespace kuki
namespace std {
template <>
struct hash<kuki::GLMaterial> {
  auto operator()(const kuki::GLMaterial &material) const noexcept -> size_t {
    size_t h{};
    kuki::hash_combine(h, hash<int>{}(material.textures.albedo));
    kuki::hash_combine(h, hash<int>{}(material.textures.normal));
    kuki::hash_combine(h, hash<int>{}(material.textures.metalness));
    kuki::hash_combine(h, hash<int>{}(material.textures.occlusion));
    kuki::hash_combine(h, hash<int>{}(material.textures.roughness));
    kuki::hash_combine(h, hash<int>{}(material.textures.specular));
    kuki::hash_combine(h, hash<int>{}(material.textures.emissive));
    kuki::hash_combine(h, hash<size_t>{}(material.fallback.textureMask.to_ullong()));
    kuki::hash_combine(h, hash<int>{}(static_cast<int>(material.type)));
    kuki::hash_combine(h, hash<int>{}(static_cast<int>(material.fallback.alphaMode)));
    kuki::hash_combine(h, hash<float>{}(material.fallback.alphaCutoff));
    kuki::hash_combine(h, hash<float>{}(material.fallback.transmission));
    kuki::hash_combine(h, hash<float>{}(material.fallback.thickness));
    kuki::hash_combine(h, hash<float>{}(material.fallback.ior));
    kuki::hash_combine(h, hash<float>{}(material.fallback.attenuation.w));
    return h;
  }
};
} // namespace std
