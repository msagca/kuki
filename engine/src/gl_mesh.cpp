#include <gl_mesh.hpp>
namespace kuki {
auto GLMesh::operator==(const GLMesh &other) const -> bool {
  return vao == other.vao;
}
GLMesh::operator bool() const {
  return vao != 0;
}
} // namespace kuki
