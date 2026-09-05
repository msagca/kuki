#include <glad/glad.h>
#include <gl_renderbuffer_pool.hpp>
#include <target_description.hpp>
namespace kuki {
auto GLRenderbufferPool::Allocate(const TargetDescription &desc) -> unsigned int {
  unsigned int renderbuffer;
  glGenRenderbuffers(1, &renderbuffer);
  Reallocate(desc, renderbuffer);
  return renderbuffer;
}
auto GLRenderbufferPool::Deallocate(unsigned int &renderbuffer) -> void {
  glDeleteRenderbuffers(1, &renderbuffer);
  renderbuffer = 0;
}
auto GLRenderbufferPool::Reallocate(const TargetDescription &desc, unsigned int &renderbuffer) -> void {
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
