#include <framebuffer_pool.hpp>
#include <pool.hpp>
//
#include <glad/glad.h>
namespace kuki {
FramebufferPool::~FramebufferPool() {
  Clear();
}
auto FramebufferPool::Clear() -> void {
  for (auto id : pool)
    glDeleteFramebuffers(1, &id);
}
auto FramebufferPool::Allocate() -> unsigned int {
  unsigned int framebuffer;
  glGenFramebuffers(1, &framebuffer);
  return framebuffer;
}
} // namespace kuki
