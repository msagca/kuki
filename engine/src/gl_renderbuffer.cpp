#include <gl_renderbuffer.hpp>
namespace kuki {
GLRenderbuffer::operator bool() const {
  return id != 0;
}
} // namespace kuki
