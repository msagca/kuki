#include <gl_framebuffer_pool.hpp>
#include <glad/glad.h>
namespace kuki {
auto GLFramebufferPool::Allocate() -> unsigned int {
  unsigned int framebuffer;
  glGenFramebuffers(1, &framebuffer);
  return framebuffer;
}
auto GLFramebufferPool::Deallocate(unsigned int &framebuffer) -> void {
  glDeleteFramebuffers(1, &framebuffer);
  framebuffer = 0;
}
} // namespace kuki
