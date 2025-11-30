#include <glad/glad.h>
#include <pool.hpp>
#include <renderbuffer_pool.hpp>
#include <texture_params.hpp>
namespace kuki {
RenderbufferPool::~RenderbufferPool() {
  Clear();
}
void RenderbufferPool::Clear() {
  for (const auto& [params, renderbuffers] : pool)
    for (auto id : renderbuffers)
      glDeleteRenderbuffers(1, &id);
  Pool::Clear();
}
GLuint RenderbufferPool::Allocate(const TextureParams& params) {
  GLuint renderbuffer;
  glGenRenderbuffers(1, &renderbuffer);
  Reallocate(params, renderbuffer);
  return renderbuffer;
}
void RenderbufferPool::Reallocate(const TextureParams& params, GLuint& renderbuffer) {
  glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
  if (params.samples > 1)
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, params.samples, GL_DEPTH24_STENCIL8, params.width, params.height);
  else
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, params.width, params.height);
  glBindRenderbuffer(GL_RENDERBUFFER, 0);
}
} // namespace kuki
