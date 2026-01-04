#pragma once
#include <gl_material.hpp>
#include <gl_mesh.hpp>
#include <hash_utils.hpp>
namespace kuki {
struct KUKI_ENGINE_API MeshMaterial {
  GLMesh mesh{};
  GLMaterial material{};
  auto operator==(const MeshMaterial &) const -> bool;
};
} // namespace kuki
namespace std {
template <>
struct hash<kuki::MeshMaterial> {
  auto operator()(const kuki::MeshMaterial &meshMat) const noexcept -> size_t {
    size_t h{};
    kuki::hash_combine(h, hash<kuki::GLMesh>{}(meshMat.mesh));
    kuki::hash_combine(h, hash<kuki::GLMaterial>{}(meshMat.material));
    return h;
  }
};
} // namespace std
