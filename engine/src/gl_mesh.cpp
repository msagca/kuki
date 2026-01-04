#include <gl_mesh.hpp>
#include <utility>
namespace kuki {
GLMesh::GLMesh()
  : BufferObject(std::in_place_type<GLMesh>) {}
auto GLMesh::operator==(const GLMesh &other) const -> bool {
  return vao == other.vao;
}
GLMesh::operator bool() const {
  return vao != 0;
}
} // namespace kuki
