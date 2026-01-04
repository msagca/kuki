#include <buffer_description.hpp>
#include <buffer_pool.hpp>
#include <glad/glad.h>
namespace kuki {
BufferPool::~BufferPool() {
  Clear();
}
auto BufferPool::Allocate(const BufferDescription &desc) -> unsigned int {
  unsigned int ubo;
  glCreateBuffers(1, &ubo);
  Reallocate(desc, ubo);
  return ubo;
}
auto BufferPool::Reallocate(const BufferDescription &desc, unsigned int &ubo) -> void {
  if (ubo == 0)
    return;
  glNamedBufferData(ubo, desc.size, nullptr, GL_DYNAMIC_DRAW);
}
void BufferPool::Clear() {
  for (const auto &[desc, buffers] : pool)
    for (auto id : buffers)
      glDeleteBuffers(1, &id);
}
} // namespace kuki
