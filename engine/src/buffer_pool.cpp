#include <buffer_description.hpp>
#include <buffer_pool.hpp>
#include <glad/glad.h>
namespace kuki {
BufferPool::~BufferPool() {
  Clear();
}
auto BufferPool::Allocate(const BufferDescription &desc) -> unsigned int {
  unsigned int buffer;
  glCreateBuffers(1, &buffer);
  Reallocate(desc, buffer);
  return buffer;
}
auto BufferPool::Reallocate(const BufferDescription &desc, unsigned int &buffer) -> void {
  if (buffer == 0)
    return;
  glNamedBufferData(buffer, desc.size, nullptr, GL_DYNAMIC_DRAW);
}
void BufferPool::Clear() {
  for (const auto &[_, buffers] : pool)
    for (const auto &id : buffers)
      glDeleteBuffers(1, &id);
}
} // namespace kuki
