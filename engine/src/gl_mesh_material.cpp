#include <gl_mesh_material.hpp>
namespace kuki {
GLMeshMat::operator bool() const {
  return mesh.vao != 0;
};
auto GLMeshMat::operator==(const GLMeshMat &other) const -> bool {
  return mesh == other.mesh && material == other.material;
};
} // namespace kuki
