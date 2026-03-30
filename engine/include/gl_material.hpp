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
    return h;
  }
};
} // namespace std
