#include <framebuffer_pool.hpp>
//
#include <glad/glad.h>
namespace kuki {
auto FramebufferPool::Allocate() -> unsigned int {
  unsigned int framebuffer;
  glGenFramebuffers(1, &framebuffer);
  return framebuffer;
}
auto FramebufferPool::Clear() -> void {
  for (const auto &id : pool)
    glDeleteFramebuffers(1, &id);
}
} // namespace kuki
