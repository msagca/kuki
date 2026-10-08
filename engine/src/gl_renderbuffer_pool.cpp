#include <algorithm>
#include <glad/glad.h>
#include <gl_format.hpp>
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
  // Multisample storage whenever the colour attachment beside this one is a multisample texture,
  // rather than whenever the count is above one. A framebuffer is only complete while every
  // attachment reports the same sample count, and the two answers part company at exactly one
  // sample: a `Texture2DMulti` target asked for one still has one sample, while plain
  // `glRenderbufferStorage` has none. Asking `TargetTypeToGL` is how the pair stays in step --
  // see the note on it for why the declared type and not the count is what decides.
  if (TargetTypeToGL(desc) == GL_TEXTURE_2D_MULTISAMPLE)
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, std::max(1, desc.samples), format, desc.width, desc.height);
  else
    glRenderbufferStorage(GL_RENDERBUFFER, format, desc.width, desc.height);
  glBindRenderbuffer(GL_RENDERBUFFER, 0);
}
} // namespace kuki
