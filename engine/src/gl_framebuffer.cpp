#include <gl_framebuffer.hpp>
#include <utility>
namespace kuki {
GLFramebuffer::GLFramebuffer()
  : RenderTarget(std::in_place_type<GLFramebuffer>) {}
GLFramebuffer::operator bool() const {
  return id != 0;
}
} // namespace kuki
