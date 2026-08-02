#include <buffer_pool.hpp>
#include <glad/glad.h>
namespace kuki {
auto BufferPool::Allocate(const int &size) -> unsigned int {
  unsigned int buffer;
  glCreateBuffers(1, &buffer);
  if (size > 0)
    Reallocate(size, buffer);
  return buffer;
}
auto BufferPool::Clear() -> void {
  for (const auto &[size, buffers] : pool)
    for (const auto &id : buffers)
      glDeleteBuffers(1, &id);
}
auto BufferPool::Reallocate(const int &size, unsigned int &buffer) -> void {
  glNamedBufferData(buffer, size, nullptr, GL_DYNAMIC_DRAW);
}
} // namespace kuki
