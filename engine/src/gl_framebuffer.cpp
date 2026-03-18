#include <gl_framebuffer.hpp>
namespace kuki {
GLFramebuffer::operator bool() const {
  return id != 0;
}
} // namespace kuki
