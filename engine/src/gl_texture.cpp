#include <gl_texture.hpp>
#include <utility>
namespace kuki {
GLTexture::GLTexture()
  : RenderTarget(std::in_place_type<GLTexture>) {}
GLTexture::operator bool() const {
  return id != 0;
}
} // namespace kuki
