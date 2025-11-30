#include <framebuffer_pool.hpp>
#include <glad/glad.h>
#include <pool.hpp>
#include <texture_params.hpp>
namespace kuki {
FramebufferPool::~FramebufferPool() {
  Clear();
}
void FramebufferPool::Clear() {
  for (const auto& [params, framebuffers] : pool)
    for (auto id : framebuffers)
      glDeleteFramebuffers(1, &id);
  Pool::Clear();
}
GLuint FramebufferPool::Allocate(const TextureParams& params) {
  GLuint framebuffer;
  glGenFramebuffers(1, &framebuffer);
  return framebuffer;
}
} // namespace kuki
