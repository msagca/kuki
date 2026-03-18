#include <gl_texture.hpp>
namespace kuki {
GLTexture::operator bool() const {
  return id != 0;
}
} // namespace kuki
