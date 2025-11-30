#include <buffer_params.hpp>
#include <gl_constants.hpp>
#include <glad/glad.h>
#include <pool.hpp>
#include <uniform_buffer_pool.hpp>
namespace kuki {
BufferParams::BufferParams(GLConst::SIZEIPTR size)
  : size(size) {}
bool BufferParams::operator==(const BufferParams& other) const {
  return size == other.size;
}
UniformBufferPool::~UniformBufferPool() {
  Clear();
}
void UniformBufferPool::Clear() {
  for (const auto& [params, buffers] : pool)
    for (auto id : buffers)
      glDeleteBuffers(1, &id);
  Pool::Clear();
}
GLuint UniformBufferPool::Allocate(const BufferParams& params) {
  GLuint ubo;
  glCreateBuffers(1, &ubo);
  Reallocate(params, ubo);
  return ubo;
}
void UniformBufferPool::Reallocate(const BufferParams& params, GLuint& ubo) {
  glNamedBufferData(ubo, params.size, nullptr, GL_DYNAMIC_DRAW);
}
} // namespace kuki
