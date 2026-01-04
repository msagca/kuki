#include <compute_type.hpp>
#include <gl_compute_shader.hpp>
#include <glad/glad.h>
namespace kuki {
GLComputeShader::GLComputeShader()
  : GLShaderBase(std::in_place_type<GLComputeShader>) {}
auto GLComputeShader::Dispatch(const unsigned int x, const unsigned int y, const unsigned int z) const -> void {
  glDispatchCompute(x, y, z);
  glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
}
} // namespace kuki
