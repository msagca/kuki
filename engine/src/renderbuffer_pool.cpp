#include <pool.hpp>
#include <renderbuffer_pool.hpp>
//
#include <glad/glad.h>
namespace kuki {
RenderbufferPool::~RenderbufferPool() {
  Clear();
}
auto RenderbufferPool::Clear() -> void {
  for (const auto &[params, renderbuffers] : pool)
    for (auto id : renderbuffers)
      glDeleteRenderbuffers(1, &id);
}
auto RenderbufferPool::Allocate(const TargetDescription &desc) -> unsigned int {
  unsigned int renderbuffer;
  glGenRenderbuffers(1, &renderbuffer);
  Reallocate(desc, renderbuffer);
  return renderbuffer;
}
auto RenderbufferPool::Reallocate(const TargetDescription &desc, unsigned int &renderbuffer) -> void {
  if (renderbuffer == 0)
    return;
  const auto format = GL_DEPTH24_STENCIL8;
  glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
  if (desc.samples > 1)
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, desc.samples, format, desc.width, desc.height);
  else
    glRenderbufferStorage(GL_RENDERBUFFER, format, desc.width, desc.height);
  glBindRenderbuffer(GL_RENDERBUFFER, 0);
}
} // namespace kuki
