#include <gl_buffer_pool.hpp>
#include <glad/glad.h>
namespace kuki {
auto GLBufferPool::Allocate(const int &size) -> unsigned int {
  unsigned int buffer;
  glCreateBuffers(1, &buffer);
  if (size > 0)
    Reallocate(size, buffer);
  return buffer;
}
auto GLBufferPool::Deallocate(unsigned int &buffer) -> void {
  glDeleteBuffers(1, &buffer);
  buffer = 0;
}
auto GLBufferPool::Reallocate(const int &size, unsigned int &buffer) -> void {
  glNamedBufferData(buffer, size, nullptr, GL_DYNAMIC_DRAW);
}
} // namespace kuki
