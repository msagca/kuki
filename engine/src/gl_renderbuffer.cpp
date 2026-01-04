#include <gl_renderbuffer.hpp>
#include <utility>
namespace kuki {
GLRenderbuffer::GLRenderbuffer()
  : RenderTarget(std::in_place_type<GLRenderbuffer>) {}
GLRenderbuffer::operator bool() const {
  return id != 0;
}
} // namespace kuki
