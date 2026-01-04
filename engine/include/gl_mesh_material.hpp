#pragma once
#include <gl_material.hpp>
#include <gl_mesh.hpp>
#include <hash_utils.hpp>
namespace kuki {
struct GLMeshMat {
  GLMesh mesh;
  GLMaterial material;
  auto operator==(const GLMeshMat &) const -> bool;
  explicit operator bool() const;
};
} // namespace kuki
namespace std {
template <>
struct hash<kuki::GLMeshMat> {
  auto operator()(const kuki::GLMeshMat &meshMat) const noexcept -> size_t {
    size_t h{};
    kuki::hash_combine(h, hash<kuki::GLMesh>{}(meshMat.mesh));
    kuki::hash_combine(h, hash<kuki::GLMaterial>{}(meshMat.material));
    return h;
  }
};
} // namespace std
